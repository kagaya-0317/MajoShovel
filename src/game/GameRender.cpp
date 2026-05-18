#include "game/GameInternal.hpp"

namespace majo {

namespace {

float easeOutCubic(float t)
{
    t = clamp(t, 0.0f, 1.0f);
    const float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

float easeOutBack(float t)
{
    t = clamp(t, 0.0f, 1.0f);
    constexpr float C1 = 1.70158f;
    constexpr float C3 = C1 + 1.0f;
    const float shifted = t - 1.0f;
    return 1.0f + C3 * shifted * shifted * shifted + C1 * shifted * shifted;
}

float smootherStep(float t)
{
    t = clamp(t, 0.0f, 1.0f);
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}


float dungeonRingIntroItemLocalProgress(float introProgress, int itemIndex, int ringIndex)
{
    const float delay = std::min(0.38f, static_cast<float>(itemIndex) * 0.045f + static_cast<float>(ringIndex) * 0.07f);
    return clamp((introProgress - delay) / 0.46f, 0.0f, 1.0f);
}

float dungeonRingIntroItemReveal(float introProgress, int itemIndex, int ringIndex)
{
    return smootherStep(dungeonRingIntroItemLocalProgress(introProgress, itemIndex, ringIndex));
}

float dungeonRingIntroOrbitScale(float introProgress)
{
    if (introProgress < 0.56f) {
        return lerp(0.12f, 1.16f, easeOutBack(introProgress / 0.56f));
    }
    return lerp(1.16f, 1.0f, easeOutCubic((introProgress - 0.56f) / 0.44f));
}

Vec2 dungeonRingIntroItemGroundPosition(
    const SpellRingSystem& spellRing,
    const SpellRingItem& item,
    float introProgress,
    int itemIndex,
    int ringIndex)
{
    const Vec2 targetOffset = item.worldPosition - spellRing.center();
    const float targetDistance = length(targetOffset);
    const Vec2 direction = targetDistance > 0.001f
        ? targetOffset * (1.0f / targetDistance)
        : fromAngle(item.localAngle);
    const float local = dungeonRingIntroItemLocalProgress(introProgress, itemIndex, ringIndex);
    const float radial = targetDistance * lerp(0.08f, 1.0f, easeOutBack(local));
    return spellRing.center() + direction * radial;
}

constexpr int RingPlaceColumns = 8;
constexpr int RingPlaceRows = 3;
constexpr int RingPlaceSlotCount = RingPlaceColumns * RingPlaceRows;
constexpr float RingPlaceSlotSize = 64.0f;
constexpr float RingPlaceSlotGap = 12.0f;
constexpr float RingPlaceSlotImageMaxSize = 48.0f;

std::array<UiCommandMenuItem, 1> ringCommandItems(bool placeCommand, bool enabled)
{
    return {{{placeCommand ? "アイテムを配置" : "リングから外す", enabled}}};
}

UiRect ringArrangeButtonRect()
{
    const UiRect panel = ringPanelRect();
    return {{panel.pos.x + 48.0f, panel.pos.y + 196.0f}, {118.0f, ui::ButtonHeight}};
}

UiRect ringPlaceWindowRect()
{
    const UiRect panel = ringPanelRect();
    return {{panel.pos.x + 120.0f, panel.pos.y + 84.0f}, {900.0f, 438.0f}};
}

UiRect ringPlaceSlotRect(int index)
{
    const UiRect panel = ringPlaceWindowRect();
    const int row = index / RingPlaceColumns;
    const int column = index % RingPlaceColumns;
    return {{
        panel.pos.x + 32.0f + static_cast<float>(column) * (RingPlaceSlotSize + RingPlaceSlotGap),
        panel.pos.y + 94.0f + static_cast<float>(row) * (RingPlaceSlotSize + RingPlaceSlotGap),
    }, {RingPlaceSlotSize, RingPlaceSlotSize}};
}

UiRect ringPlaceDetailRect()
{
    const UiRect panel = ringPlaceWindowRect();
    return {{panel.pos.x + 648.0f, panel.pos.y + 94.0f}, {220.0f, 256.0f}};
}

InventoryUiEntryView ringPlaceEntryView(const InventorySystem& inventory, int slotIndex)
{
    InventoryUiEntryView entry{};
    if (const InventoryObjectStack* stack = inventory.screenObjectStackAt(slotIndex)) {
        entry.item = &stack->item;
        entry.stackCount = stack->count;
    } else if (const InventoryObjectInstance* instance = inventory.screenObjectInstanceAt(slotIndex)) {
        entry.item = &instance->item;
        entry.instance = &instance->instance;
        entry.stackCount = 1;
    }
    return entry;
}

bool ringPlaceSlotEnabled(
    const InventorySystem& inventory,
    const SpellRingSystem& spellRing,
    int slotIndex,
    float localAngle)
{
    return inventory.hasScreenItemAt(slotIndex) &&
        inventory.screenItemCanAddToRing(slotIndex, spellRing, localAngle);
}

int firstRingPlaceableSlot(
    const InventorySystem& inventory,
    const SpellRingSystem& spellRing,
    float localAngle)
{
    const int count = std::min(inventory.screenSlotCount(), RingPlaceSlotCount);
    for (int i = 0; i < count; ++i) {
        if (ringPlaceSlotEnabled(inventory, spellRing, i, localAngle)) {
            return i;
        }
    }
    return -1;
}

int movedRingPlaceSelection(
    const InventorySystem& inventory,
    const SpellRingSystem& spellRing,
    float localAngle,
    int current,
    int delta)
{
    const int count = std::min(inventory.screenSlotCount(), RingPlaceSlotCount);
    if (count <= 0 || delta == 0) {
        return current;
    }

    int cursor = std::clamp(current, 0, count - 1);
    for (int step = 0; step < count; ++step) {
        cursor = (cursor + delta) % count;
        if (cursor < 0) {
            cursor += count;
        }
        if (ringPlaceSlotEnabled(inventory, spellRing, cursor, localAngle)) {
            return cursor;
        }
    }
    return current;
}

bool ringInventoryHasAnyItem(const InventorySystem& inventory)
{
    const int count = std::min(inventory.screenSlotCount(), RingPlaceSlotCount);
    for (int i = 0; i < count; ++i) {
        if (inventory.hasScreenItemAt(i)) {
            return true;
        }
    }
    return false;
}

bool ringInventoryHasAnyAutoPlaceableItem(const InventorySystem& inventory, const SpellRingSystem& spellRing)
{
    const int count = std::min(inventory.screenSlotCount(), RingPlaceSlotCount);
    for (int i = 0; i < count; ++i) {
        if (inventory.screenItemCanAddToRing(i, spellRing, std::nullopt)) {
            return true;
        }
    }
    return false;
}

std::string ringPlacementUnavailableStatus(const InventorySystem& inventory, const SpellRingSystem& spellRing)
{
    if (!ringInventoryHasAnyItem(inventory)) {
        return "配置できるアイテムがありません";
    }
    if (!spellRing.canAddItem()) {
        return "リング満杯です";
    }
    if (!ringInventoryHasAnyAutoPlaceableItem(inventory, spellRing)) {
        return "重量上限のため配置できません";
    }
    return "この位置には配置できません";
}

bool ringPlacementHitAreaContains(Vec2 point, const SpellRingSystem& spellRing, const RuntimeBalance& balance)
{
    if (!ringPanelRect().contains(point) || ringDetailRect().contains(point)) {
        return false;
    }

    std::vector<Vec2> orbitPath = getRingPathSamplePoints(
        spellRing.center(),
        ringUiOrbitContext(spellRing, balance, 0, 1),
        160);
    float nearestDistanceSq = std::numeric_limits<float>::max();
    for (Vec2 pathPoint : orbitPath) {
        pathPoint = applyRingUiShapeRotation(spellRing, ringWorldToUi(spellRing, pathPoint));
        nearestDistanceSq = std::min(nearestDistanceSq, distanceSquared(point, pathPoint));
    }
    constexpr float HitDistance = 72.0f;
    return nearestDistanceSq <= HitDistance * HitDistance;
}

void drawRingPlaceWindow(
    Renderer& renderer,
    const InventorySystem& inventory,
    const ObjectCatalog& objectCatalog,
    const EncyclopediaSystem& encyclopedia,
    const SpellRingSystem& spellRing,
    int selection,
    float localAngle,
    std::string_view status)
{
    const UiRect panel = ringPlaceWindowRect();
    UiWindowScope placeWindow(
        renderer,
        "ring.place",
        panel,
        "アイテム配置",
        "WASD/矢印 選択  F/Enter 配置  Esc/右クリック 戻る",
        UiWindowOptions{true, true});

    const int slotCount = std::min(inventory.screenSlotCount(), RingPlaceSlotCount);
    for (int i = 0; i < slotCount; ++i) {
        const InventoryUiEntryView entry = ringPlaceEntryView(inventory, i);
        const bool hasItem = entry.item != nullptr;
        const bool enabled = ringPlaceSlotEnabled(inventory, spellRing, i, localAngle);
        InventoryUiSlotStyle style{i == selection && enabled, hasItem && !enabled, RingPlaceSlotImageMaxSize};
        if (entry.item != nullptr && entry.instance == nullptr && entry.stackCount > 1) {
            style.showTopRightCount = true;
            style.topRightCount = entry.stackCount;
        }
        drawInventoryUiSlot(renderer, ringPlaceSlotRect(i), entry, style);
    }

    InventoryUiEntryView detailEntry{};
    if (selection >= 0 && selection < slotCount) {
        detailEntry = ringPlaceEntryView(inventory, selection);
    }
    drawInventoryUiDetailPanel(renderer, ringPlaceDetailRect(), detailEntry, objectCatalog, encyclopedia, true);

    if (!status.empty()) {
        renderer.drawText(panel.pos + Vec2{32.0f, panel.size.y - 66.0f}, status, {255, 230, 150, 255}, 2);
    }
}

float cometArrangeArcRadians(const RingOrbitTuning& tuning)
{
    const float maxArcDegrees = std::clamp(tuning.cometMaxArcDegrees, 10.0f, 360.0f);
    return std::clamp(std::abs(tuning.cometArcDegrees), 10.0f, maxArcDegrees) * (Pi / 180.0f);
}

bool arrangeActiveRingItemsEvenly(SpellRingSystem& spellRing, const RuntimeBalance& balance)
{
    std::vector<SpellRingItem>& items = spellRing.items();
    const int count = static_cast<int>(items.size());
    if (count <= 0) {
        return false;
    }

    const RingShape shape = spellRing.activeRingShape();
    const RingOrbitTuning tuning = makeRingOrbitTuning(balance);
    const float full = Pi * 2.0f;
    const int ringIndex = spellRing.activeRingIndex();
    for (int i = 0; i < count; ++i) {
        float angle = 0.0f;
        if (shape == RingShape::Comet) {
            const float arc = cometArrangeArcRadians(tuning);
            angle = -arc * 0.5f + arc * (static_cast<float>(i) + 0.5f) / static_cast<float>(count);
        } else if (shape == RingShape::FigureEight) {
            angle = full * (static_cast<float>(i) + 0.25f) / static_cast<float>(count);
        } else {
            angle = -Pi * 0.5f + full * static_cast<float>(i) / static_cast<float>(count);
        }

        items[static_cast<std::size_t>(i)].ringIndex = ringIndex;
        items[static_cast<std::size_t>(i)].localAngle = spellRing.quantizeLocalAngle(angle, balance);
    }
    return true;
}

ItemInstance inventoryInstanceFromRingItem(
    InventorySystem& inventory,
    const ObjectCatalog& objectCatalog,
    const SpellRingItem& item)
{
    ItemInstance instance;
    if (item.instanceId.empty()) {
        const ItemData* object = objectCatalog.registry.findById(item.objectId);
        const ItemData missingObject = object == nullptr ? makeMissingItemData(item.objectId) : ItemData{};
        instance = inventory.createDetachedObjectInstance(object != nullptr ? *object : missingObject);
    } else {
        instance.instanceId = item.instanceId;
        instance.objectId = item.objectId;
    }

    instance.objectId = item.objectId;
    instance.currentDurability = item.durability;
    instance.maxDurability = item.maxDurability;
    instance.enhanceLevel = item.enhanceLevel;
    instance.attackBonus = item.attackBonus;
    instance.digBonus = item.digBonus;
    instance.durabilityBonus = item.durabilityBonus;
    instance.weightModifier = item.weightModifier;
    instance.sizeModifier = item.sizeModifier;
    instance.protectionEnabled = item.protectionEnabled;
    instance.isBroken = item.broken();
    instance.addedEffects = item.addedEffects;
    instance.addedTags = item.addedTags;
    return instance;
}

bool returnRingItemToInventory(
    InventorySystem& inventory,
    const ObjectCatalog& objectCatalog,
    const SpellRingItem& item)
{
    if (item.objectId.empty()) {
        return false;
    }

    return inventory.addObjectInstance(
        objectCatalog,
        inventoryInstanceFromRingItem(inventory, objectCatalog, item));
}

bool removeRingItemToInventory(
    std::vector<SpellRingItem>& items,
    int& selection,
    InventorySystem& inventory,
    const ObjectCatalog& objectCatalog,
    std::string& status)
{
    if (selection < 0 || selection >= static_cast<int>(items.size())) {
        status = "アイテム未選択です";
        return false;
    }

    const SpellRingItem& selectedItem = items[static_cast<std::size_t>(selection)];
    if (selectedItem.objectId.empty()) {
        status = "このアイテムは外せません";
        return false;
    }
    if (!returnRingItemToInventory(inventory, objectCatalog, selectedItem)) {
        status = "インベントリ満杯のため外せません";
        return false;
    }

    items.erase(items.begin() + selection);
    selection = std::min(selection, std::max(0, static_cast<int>(items.size()) - 1));
    status = "インベントリへ戻しました";
    return true;
}

void drawDungeonRingIntroOrbit(
    Renderer& renderer,
    const SpellRingSystem& spellRing,
    const RuntimeBalance& balance,
    float introProgress,
    float totalSeconds)
{
    const Vec2 center = spellRing.center();
    const float appear = smootherStep(introProgress / 0.24f);
    const float burstFade = 1.0f - smootherStep((introProgress - 0.08f) / 0.62f);
    const float orbitScale = dungeonRingIntroOrbitScale(introProgress);

    renderer.fillSoftCircle(center, 26.0f + 54.0f * appear, withAlpha({150, 220, 255, 255}, 86.0f * burstFade));
    renderer.drawSoftRing(
        center,
        spellRing.radius() * lerp(0.24f, 1.34f, easeOutCubic(introProgress)),
        18.0f * burstFade,
        withAlpha({128, 224, 255, 255}, 118.0f * burstFade));
    renderer.drawSoftRing(
        center,
        spellRing.radius() * lerp(0.10f, 0.94f, easeOutBack(clamp(introProgress / 0.48f, 0.0f, 1.0f))),
        8.0f + 10.0f * burstFade,
        withAlpha({214, 240, 255, 255}, 92.0f * burstFade));

    for (RingShape shapePass : MagicRingShapeRenderOrder) {
        for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
            const auto& ringItems = spellRing.itemsForRing(ringIndex);
            if (ringItems.empty() || spellRing.ringShapeForIndex(ringIndex) != shapePass) {
                continue;
            }

            std::vector<Vec2> orbitPath = shapePass == RingShape::Circle
                ? makeCirclePath(center, spellRing.radius() * orbitScale, 176)
                : spellRing.pathSamplePointsForRing(ringIndex, center, orbitScale, balance, 176);
            drawMagicOrbitPath(
                renderer,
                orbitPath,
                center,
                MagicOrbitDrawOptions{
                    shapePass,
                    ringIndex == spellRing.activeRingIndex(),
                    false,
                    true,
                    false,
                    ringIndex,
                    totalSeconds + introProgress * 2.6f,
                    (0.16f + 1.18f * appear) * (ringIndex == spellRing.activeRingIndex() ? 1.0f : 0.72f),
                });
        }
    }

    drawMagicStar(
        renderer,
        center,
        11.0f + 8.0f * burstFade,
        withAlpha({214, 240, 255, 255}, 168.0f * std::max(appear, burstFade)),
        totalSeconds * 1.2f);
}

void drawDungeonRingIntroItem(
    Renderer& renderer,
    const SpellRingSystem& spellRing,
    const ObjectCatalog& objectCatalog,
    const SpellRingItem& item,
    int itemIndex,
    float introProgress,
    float totalSeconds)
{
    const int ringIndex = std::clamp(item.ringIndex, 0, SpellRingCount - 1);
    const float local = dungeonRingIntroItemLocalProgress(introProgress, itemIndex, ringIndex);
    if (local <= 0.0f) {
        return;
    }

    const float reveal = smootherStep(local);
    const Vec2 introGround = dungeonRingIntroItemGroundPosition(spellRing, item, introProgress, itemIndex, ringIndex);
    const Vec2 toItem = introGround - spellRing.center();
    const float lift = (1.0f - reveal) * 22.0f + std::sin(local * Pi) * 10.0f;
    const Vec2 drawPosition = elevatedDrawPosition(introGround, ringItemAltitude(item, totalSeconds) + lift);
    const RingShape ringShape = spellRing.ringShapeForIndex(ringIndex);
    const int ringItemCount = static_cast<int>(spellRing.itemsForRing(ringIndex).size());
    const float cometVisualScale = ringShape == RingShape::Comet
        ? std::clamp(1.0f - std::max(0, ringItemCount - 10) * 0.014f, 0.76f, 1.0f)
        : 1.0f;
    const float popScale = cometVisualScale * (lerp(0.56f, 1.0f, reveal) + std::sin(local * Pi) * 0.16f * (1.0f - local));
    const unsigned char alpha = alphaByte(255.0f * reveal);
    const Color tint{255, 255, 255, alpha};

    renderer.drawActorShadow(
        actorShadowAnchor(introGround, ItemShadowGroundOffsetY),
        ringItemShadowVisualSize(item, totalSeconds) * popScale,
        {0, 0, 0, static_cast<unsigned char>(std::clamp(std::lround(74.0f * reveal), 0L, 255L))});

    const ItemData* object = objectForRingItem(objectCatalog, item);
    const auto drawObject = [&]() {
        if (object == nullptr) {
            return false;
        }
        ObjectImageDrawOptions options;
        options.tint = tint;
        options.outlineColor.a = alpha;
        options.scaleMultiplier = popScale / std::max(0.001f, cometVisualScale);
        return drawObjectImage(
            renderer,
            *object,
            drawPosition,
            {RingObjectImageMaxSize * cometVisualScale, RingObjectImageMaxSize * cometVisualScale},
            options);
    };

    if (item.type == SpellRingItemType::Shovel) {
        if (renderer.hasIconSheet()) {
            const float iconSize = IconDrawSize * popScale;
            renderer.drawIcon(ShovelIconIndex, drawPosition - Vec2{iconSize * 0.5f, iconSize * 0.5f}, {iconSize, iconSize}, tint);
        } else if (!drawObject()) {
            renderer.fillCircle(drawPosition, item.hitRadius * popScale, withAlpha({178, 184, 190, 255}, 255.0f * reveal));
            const Vec2 outward = normalize(introGround - spellRing.center());
            renderer.drawLine(drawPosition, drawPosition + outward * (15.0f * popScale), withAlpha({90, 96, 102, 255}, 255.0f * reveal));
        }
    } else if (item.type == SpellRingItemType::Torch) {
        if (renderer.hasIconSheet()) {
            const float iconSize = IconDrawSize * popScale;
            renderer.drawIcon(TorchIconIndex, drawPosition - Vec2{iconSize * 0.5f, iconSize * 0.5f}, {iconSize, iconSize}, tint);
        } else if (!drawObject()) {
            renderer.fillCircle(drawPosition, item.hitRadius * popScale, withAlpha({242, 122, 25, 255}, 255.0f * reveal));
            renderer.fillCircle(drawPosition + Vec2{2.0f, -2.0f} * popScale, 4.0f * popScale, withAlpha({255, 238, 98, 255}, 255.0f * reveal));
        }
    } else if (!drawObject()) {
        renderer.fillCircle(drawPosition, item.hitRadius * popScale, withAlpha({96, 122, 210, 255}, 255.0f * reveal));
        renderer.drawCircle(drawPosition, item.hitRadius * popScale + 3.0f, withAlpha({160, 202, 255, 255}, 255.0f * reveal));
    }

    const float sparkle = std::sin(local * Pi);
    if (sparkle > 0.05f) {
        const Vec2 introOffset = introGround - spellRing.center();
        const Vec2 outward = lengthSquared(introOffset) > 0.0001f ? normalize(introOffset) : Vec2{0.0f, -1.0f};
        drawMagicStar(
            renderer,
            drawPosition - outward * (10.0f + 12.0f * sparkle),
            3.0f + sparkle * 5.0f,
            withAlpha({255, 246, 190, 255}, 160.0f * sparkle * reveal),
            totalSeconds * 2.0f + static_cast<float>(itemIndex));
    }

    if (item.hiddenDetectionRadius > 0.0f) {
        renderer.drawCircle(drawPosition, item.hiddenDetectionRadius * lerp(0.75f, 1.0f, reveal), withAlpha({126, 208, 255, 255}, 90.0f * reveal));
    }
    if (item.treasureDetectionRadius > 0.0f) {
        renderer.drawCircle(drawPosition, item.treasureDetectionRadius * lerp(0.75f, 1.0f, reveal), withAlpha({255, 220, 92, 255}, 90.0f * reveal));
    }
}

} // namespace
void Game::updateRingScreen(const Input& input, UiContext& ui, float dt)
{
    auto& items = spellRing_.items();
    if (ringSnapActive_) {
        if (ringDragItemIndex_ < 0 || ringDragItemIndex_ >= static_cast<int>(items.size())) {
            ringSnapActive_ = false;
            ringDragItemIndex_ = -1;
        } else {
            ringSnapElapsed_ = std::min(RingSnapDuration, ringSnapElapsed_ + dt);
            const float t = RingSnapDuration <= 0.0f ? 1.0f : clamp(ringSnapElapsed_ / RingSnapDuration, 0.0f, 1.0f);
            const float eased = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
            ringDragDisplayAngle_ = spellRing_.normalizeLocalAngle(
                ringSnapStartAngle_ + shortestRingAngleDelta(ringSnapStartAngle_, ringSnapTargetAngle_, spellRing_.runtimeRingShape(), balance_) * eased,
                balance_);
            if (t >= 1.0f) {
                items[static_cast<std::size_t>(ringDragItemIndex_)].localAngle = ringSnapTargetAngle_;
                ringSnapActive_ = false;
                ringDragItemIndex_ = -1;
            }
        }
        ui.block(ringPanelRect());
        return;
    }

    if (spellRing_.activeRingIndex() >= UnlockedRingCount) {
        spellRing_.switchActiveRing(-spellRing_.activeRingIndex());
        closeUiCommandMenu(ringCommandMenu_);
        ringCommandItemIndex_ = -1;
        ringCommandPlaceActive_ = false;
        ringPlaceModeActive_ = false;
        ringEmptyPressActive_ = false;
        ringStatus_.clear();
    }

    if (!items.empty()) {
        ringSlotSelection_ = std::clamp(ringSlotSelection_, 0, static_cast<int>(items.size()) - 1);
    } else {
        ringSlotSelection_ = 0;
    }

    if (ringCommandMenu_.open && uiCancelRequested(ringCancelState_, input, ui, ringPanelRect())) {
        closeUiCommandMenu(ringCommandMenu_);
        ringCommandItemIndex_ = -1;
        ringCommandPlaceActive_ = false;
        ui.block(ringPanelRect());
        return;
    }

    const int commandItemIndex = ringCommandItemIndex_ >= 0 ? ringCommandItemIndex_ : ringSlotSelection_;
    const bool commandCanRemove = commandItemIndex >= 0 &&
        commandItemIndex < static_cast<int>(items.size()) &&
        !items[static_cast<std::size_t>(commandItemIndex)].objectId.empty();
    const bool commandCanPlace = ringCommandPlaceActive_ &&
        firstRingPlaceableSlot(inventory_, spellRing_, ringCommandPlaceAngle_) >= 0;
    const std::array<UiCommandMenuItem, 1> commandItems = ringCommandItems(
        ringCommandPlaceActive_,
        ringCommandPlaceActive_ ? commandCanPlace : commandCanRemove);
    const int commandSelection = updateUiCommandMenu(
        ringCommandMenu_,
        ui,
        input,
        commandItems.data(),
        static_cast<int>(commandItems.size()));
    if (commandSelection >= 0) {
        if (ringCommandPlaceActive_) {
            const int firstSlot = firstRingPlaceableSlot(inventory_, spellRing_, ringCommandPlaceAngle_);
            if (firstSlot >= 0) {
                ringPlaceModeActive_ = true;
                ringPlaceTargetAngle_ = ringCommandPlaceAngle_;
                ringPlaceSelection_ = firstSlot;
                ringStatus_.clear();
            } else {
                ringStatus_ = ringPlacementUnavailableStatus(inventory_, spellRing_);
            }
        } else if (ringCommandItemIndex_ >= 0) {
            ringSlotSelection_ = ringCommandItemIndex_;
            removeRingItemToInventory(items, ringSlotSelection_, inventory_, objectCatalog_, ringStatus_);
        }
        ringCommandItemIndex_ = -1;
        ringCommandPlaceActive_ = false;
        ui.block(ringPanelRect());
        return;
    }
    if (ringCommandMenu_.open) {
        ui.block(ringPanelRect());
        return;
    }
    if (!ringCommandMenu_.visible) {
        ringCommandItemIndex_ = -1;
        ringCommandPlaceActive_ = false;
    }

    if (ringPlaceModeActive_) {
        const int slotCount = std::min(inventory_.screenSlotCount(), RingPlaceSlotCount);
        const int firstSlot = firstRingPlaceableSlot(inventory_, spellRing_, ringPlaceTargetAngle_);
        if (firstSlot < 0) {
            ringPlaceModeActive_ = false;
            ringStatus_ = ringPlacementUnavailableStatus(inventory_, spellRing_);
            ui.block(ringPanelRect());
            return;
        }
        ringPlaceSelection_ = std::clamp(ringPlaceSelection_, 0, std::max(0, slotCount - 1));
        if (!ringPlaceSlotEnabled(inventory_, spellRing_, ringPlaceSelection_, ringPlaceTargetAngle_)) {
            ringPlaceSelection_ = firstSlot;
        }

        if (uiCancelRequested(ringCancelState_, input, ui, ringPlaceWindowRect())) {
            ringPlaceModeActive_ = false;
            ringStatus_ = "配置をキャンセルしました";
            ui.block(ringPanelRect());
            return;
        }

        const auto tryPlaceSelection = [&]() {
            if (!ringPlaceSlotEnabled(inventory_, spellRing_, ringPlaceSelection_, ringPlaceTargetAngle_)) {
                ringStatus_ = "このアイテムは配置できません";
                return;
            }

            SpellRingAddResult result{};
            if (inventory_.addScreenItemToRing(
                    ringPlaceSelection_,
                    spellRing_,
                    std::optional<float>{ringPlaceTargetAngle_},
                    &result)) {
                ringSlotSelection_ = std::max(0, result.itemIndex);
                ringPlaceModeActive_ = false;
                ringStatus_ = "リングに配置しました";
            } else {
                ringStatus_ = ringPlacementUnavailableStatus(inventory_, spellRing_);
            }
        };

        if (input.pressed(InputAction::MoveLeft)) {
            ringPlaceSelection_ = movedRingPlaceSelection(inventory_, spellRing_, ringPlaceTargetAngle_, ringPlaceSelection_, -1);
        }
        if (input.pressed(InputAction::MoveRight)) {
            ringPlaceSelection_ = movedRingPlaceSelection(inventory_, spellRing_, ringPlaceTargetAngle_, ringPlaceSelection_, 1);
        }
        if (input.pressed(InputAction::MoveUp)) {
            ringPlaceSelection_ = movedRingPlaceSelection(
                inventory_,
                spellRing_,
                ringPlaceTargetAngle_,
                ringPlaceSelection_,
                -RingPlaceColumns);
        }
        if (input.pressed(InputAction::MoveDown)) {
            ringPlaceSelection_ = movedRingPlaceSelection(
                inventory_,
                spellRing_,
                ringPlaceTargetAngle_,
                ringPlaceSelection_,
                RingPlaceColumns);
        }
        if (input.shortcutCursorDelta() != 0) {
            ringPlaceSelection_ = movedRingPlaceSelection(
                inventory_,
                spellRing_,
                ringPlaceTargetAngle_,
                ringPlaceSelection_,
                input.shortcutCursorDelta());
        }

        for (int i = 0; i < slotCount; ++i) {
            const UiRect rect = ringPlaceSlotRect(i);
            const bool enabled = ringPlaceSlotEnabled(inventory_, spellRing_, i, ringPlaceTargetAngle_);
            if (enabled && rect.contains(ui.mouse())) {
                ringPlaceSelection_ = i;
            }
            if (input.mouseLeftPressed() && rect.contains(ui.mouse()) && !ui.pointerConsumed()) {
                ui.consumePointer();
                if (enabled) {
                    ringPlaceSelection_ = i;
                    tryPlaceSelection();
                } else if (inventory_.hasScreenItemAt(i)) {
                    ringStatus_ = "このアイテムは配置できません";
                }
                ui.block(ringPanelRect());
                return;
            }
        }

        if (input.confirmPressed() || input.useItemPressed()) {
            tryPlaceSelection();
            ui.block(ringPanelRect());
            return;
        }

        ui.block(ringPanelRect());
        return;
    }

    std::array<UiTabItem, UnlockedRingCount> ringTabs{};
    std::array<UiRect, UnlockedRingCount> ringTabRects{};
    std::array<std::string, UnlockedRingCount> ringTabLabels{};
    for (int i = 0; i < UnlockedRingCount; ++i) {
        ringTabLabels[static_cast<std::size_t>(i)] = "リング " + std::to_string(i + 1);
        ringTabs[static_cast<std::size_t>(i)] = {ringTabLabels[static_cast<std::size_t>(i)], true};
        ringTabRects[static_cast<std::size_t>(i)] = ringTabRect(i);
    }
    UiTabsInput ringTabsInput{};
    const int directRingFocus = input.shortcutSlotPressed();
    if (directRingFocus >= 0 && directRingFocus < UnlockedRingCount) {
        ringTabsInput.directFocusIndex = directRingFocus;
    }
    ringTabsInput.focusDelta = input.activeRingDelta();
    ringTabsInput.commit = input.confirmPressed() || input.useItemPressed();
    const int ringTabSelection = updateUiTabs(
        ringTabs_,
        ui,
        ringTabsInput,
        spellRing_.activeRingIndex(),
        ringTabs.data(),
        static_cast<int>(ringTabs.size()),
        ringTabRects.data());
    if (ringTabSelection >= 0) {
        if (ringTabSelection != spellRing_.activeRingIndex()) {
            spellRing_.switchActiveRing(ringTabSelection - spellRing_.activeRingIndex());
            closeUiCommandMenu(ringCommandMenu_);
            ringCommandItemIndex_ = -1;
            ringCommandPlaceActive_ = false;
            ringPlaceModeActive_ = false;
            ringEmptyPressActive_ = false;
            ringStatus_.clear();
        }
        return;
    }

    const bool actualRing = true;

    if (ui.pressed(ringArrangeButtonRect())) {
        closeUiCommandMenu(ringCommandMenu_);
        ringCommandItemIndex_ = -1;
        ringCommandPlaceActive_ = false;
        if (ringGrabActive_) {
            ringStatus_ = "つかみ中は整列できません";
        } else if (arrangeActiveRingItemsEvenly(spellRing_, balance_)) {
            ringSlotSelection_ = std::clamp(ringSlotSelection_, 0, static_cast<int>(items.size()) - 1);
            ringStatus_ = "等間隔に整列しました";
        } else {
            ringStatus_ = "アイテム未配置です";
        }
        ui.block(ringPanelRect());
        return;
    }

    if ((ringDragPending_ || ringDragActive_) && uiCancelRequested(ringCancelState_, input, ui, ringPanelRect())) {
        ringDragPending_ = false;
        ringDragActive_ = false;
        ringDragItemIndex_ = -1;
        ringStatus_ = "ドラッグをキャンセルしました";
        ui.block(ringPanelRect());
        return;
    }

    if (ringDragPending_ || ringDragActive_) {
        if (ringDragItemIndex_ < 0 || ringDragItemIndex_ >= static_cast<int>(items.size())) {
            ringDragPending_ = false;
            ringDragActive_ = false;
            ringDragItemIndex_ = -1;
            ui.block(ringPanelRect());
            return;
        }

        if (input.mouseLeftHeld()) {
            if (!ringDragActive_ && distanceSquared(input.mouseScreen(), ringDragStartMouse_) >= 36.0f) {
                ringDragActive_ = true;
                ringDragPending_ = false;
            }
            if (ringDragActive_) {
                ringDragDisplayAngle_ = ringAngleFromPoint(input.mouseScreen(), spellRing_, balance_);
            }
        }

        if (input.mouseLeftReleased()) {
            if (ringDragActive_) {
                const float desired = ringAngleFromPoint(input.mouseScreen(), spellRing_, balance_);
                const std::optional<float> snapAngle = spellRing_.nearestPlaceableAngle(ringDragItemIndex_, desired, RingDragSnapMaxDelta);
                ringSnapStartAngle_ = desired;
                ringSnapTargetAngle_ = snapAngle.value_or(ringDragOriginalAngle_);
                ringDragDisplayAngle_ = ringSnapStartAngle_;
                ringSnapElapsed_ = 0.0f;
                ringSnapActive_ = true;
                ringStatus_ = snapAngle ? "近い空き位置へ補正しました" : "近くに空きがないため元の位置へ戻しました";
            } else {
                ringSlotSelection_ = ringDragItemIndex_;
                ringCommandItemIndex_ = ringDragItemIndex_;
                ringCommandPlaceActive_ = false;
                const bool canRemove = !items[static_cast<std::size_t>(ringDragItemIndex_)].objectId.empty();
                const std::array<UiCommandMenuItem, 1> menuItems = ringCommandItems(false, canRemove);
                openUiCommandMenu(
                    ringCommandMenu_,
                    input.mouseScreen(),
                    ringPanelRect(),
                    static_cast<int>(menuItems.size()),
                    menuItems.data(),
                    180.0f,
                    2);
                ringStatus_.clear();
            }
            ringDragPending_ = false;
            ringDragActive_ = false;
            ui.block(ringPanelRect());
            return;
        }

        ui.block(ringPanelRect());
        return;
    }

    if (ringEmptyPressActive_) {
        if (input.mouseLeftHeld() &&
            distanceSquared(input.mouseScreen(), ringEmptyPressMouse_) >= 36.0f) {
            ringEmptyPressActive_ = false;
        }

        if (input.mouseLeftReleased()) {
            if (ringEmptyPressActive_) {
                const float placeAngle = ringEmptyPressAngle_;
                if (ringGrabActive_) {
                    ringStatus_ = "つかみ中は配置できません";
                } else if (firstRingPlaceableSlot(inventory_, spellRing_, placeAngle) < 0) {
                    ringStatus_ = ringPlacementUnavailableStatus(inventory_, spellRing_);
                } else {
                    ringCommandItemIndex_ = -1;
                    ringCommandPlaceActive_ = true;
                    ringCommandPlaceAngle_ = placeAngle;
                    const std::array<UiCommandMenuItem, 1> menuItems = ringCommandItems(true, true);
                    openUiCommandMenu(
                        ringCommandMenu_,
                        input.mouseScreen(),
                        ringPanelRect(),
                        static_cast<int>(menuItems.size()),
                        menuItems.data(),
                        190.0f,
                        2);
                    ringStatus_.clear();
                }
            }
            ringEmptyPressActive_ = false;
            ui.block(ringPanelRect());
            return;
        }
    }

    if (uiCancelRequested(ringCancelState_, input, ui, ringPanelRect())) {
        if (ringGrabActive_) {
            cancelRingGrab();
            ringStatus_ = "つかみ操作をキャンセルしました";
        } else {
            mode_ = ScreenMode::PauseMenu;
            pausePage_ = PauseMenuPage::Main;
        }
        return;
    }

    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        const UiRect rect = ringItemUiRect(items[static_cast<std::size_t>(i)], spellRing_, balance_, i, static_cast<int>(items.size()));
        if (rect.contains(ui.mouse())) {
            ringSlotSelection_ = i;
        }
        if (ui.pressed(rect)) {
            closeUiCommandMenu(ringCommandMenu_);
            ringCommandItemIndex_ = -1;
            ringCommandPlaceActive_ = false;
            ringEmptyPressActive_ = false;
            ringSlotSelection_ = i;
            ringDragPending_ = true;
            ringDragActive_ = false;
            ringDragItemIndex_ = i;
            ringDragOriginalAngle_ = items[static_cast<std::size_t>(i)].localAngle;
            ringDragDisplayAngle_ = ringDragOriginalAngle_;
            ringDragStartMouse_ = input.mouseScreen();
            return;
        }
    }

    if (input.mouseLeftPressed() &&
        !ui.pointerConsumed() &&
        ringPlacementHitAreaContains(ui.mouse(), spellRing_, balance_)) {
        closeUiCommandMenu(ringCommandMenu_);
        ringCommandItemIndex_ = -1;
        ringCommandPlaceActive_ = false;
        ringEmptyPressActive_ = true;
        ringEmptyPressMouse_ = input.mouseScreen();
        ringEmptyPressAngle_ = ringAngleFromPoint(input.mouseScreen(), spellRing_, balance_);
        ui.consumePointer();
        return;
    }

    ui.block(ringPanelRect());

    if (!items.empty() && input.shortcutCursorDelta() != 0) {
        const int count = static_cast<int>(items.size());
        ringSlotSelection_ = (ringSlotSelection_ + input.shortcutCursorDelta() + count) % count;
    }
    if (input.pressed(InputAction::MoveLeft)) {
        if (!spellRing_.moveItemAngle(ringSlotSelection_, -RingAngleStep)) {
            ringStatus_ = "その位置には移動できません";
        }
    }
    if (input.pressed(InputAction::MoveRight)) {
        if (!spellRing_.moveItemAngle(ringSlotSelection_, RingAngleStep)) {
            ringStatus_ = "その位置には移動できません";
        }
    }

    (void)actualRing;

    if (input.addRingPressed()) {
        if (ringGrabActive_) {
            ringStatus_ = "つかみ中は外せません";
        } else if (ringSlotSelection_ < static_cast<int>(items.size())) {
            removeRingItemToInventory(items, ringSlotSelection_, inventory_, objectCatalog_, ringStatus_);
        } else {
            ringStatus_ = "アイテム未選択です";
        }
        return;
    }

    if (input.pressed(InputAction::ToggleProtection)) {
        if (ringGrabActive_) {
            ringStatus_ = "つかみ中は保護変更できません";
        } else if (ringSlotSelection_ < static_cast<int>(items.size())) {
            SpellRingItem& item = items[ringSlotSelection_];
            if (item.instanceId.empty()) {
                ringStatus_ = "個体アイテムのみ保護できます";
            } else {
                item.protectionEnabled = !item.protectionEnabled;
                ringStatus_ = item.protectionEnabled ? "保護ON" : "保護OFF";
            }
        } else {
            ringStatus_ = "アイテム未選択です";
        }
        return;
    }

    if (input.grabOrPlacePressed()) {
        if (ringGrabActive_) {
            if (spellRing_.addItem(ringGrabbedItem_)) {
                ringSlotSelection_ = std::max(0, static_cast<int>(items.size()) - 1);
                ringGrabActive_ = false;
                ringGrabOrigin_ = -1;
                ringStatus_ = "最も広い空きに配置しました";
            } else {
                ringStatus_ = "配置できる空きがありません";
            }
        } else if (ringSlotSelection_ < static_cast<int>(items.size())) {
            ringGrabbedItem_ = items[ringSlotSelection_];
            ringGrabOrigin_ = ringSlotSelection_;
            items.erase(items.begin() + ringSlotSelection_);
            ringGrabActive_ = true;
            ringStatus_ = "装着アイテムをつかみました";
        } else {
            ringStatus_ = "アイテム未選択です";
        }
        return;
    }

    if (input.useItemPressed() || input.confirmPressed()) {
        if (ringGrabActive_) {
            if (spellRing_.addItem(ringGrabbedItem_)) {
                ringSlotSelection_ = std::max(0, static_cast<int>(items.size()) - 1);
                ringGrabActive_ = false;
                ringGrabOrigin_ = -1;
                ringStatus_ = "最も広い空きに配置しました";
            } else {
                ringStatus_ = "配置できる空きがありません";
            }
        } else if (ringSlotSelection_ < static_cast<int>(items.size())) {
            ringStatus_ = "詳細を表示中";
        } else {
            ringStatus_ = "アイテム未選択です";
        }
    }
}

void Game::updatePauseMenu(const Input& input, UiContext& ui)
{
    const UiRect cancelPanel = pausePage_ == PauseMenuPage::QuitConfirm ? quitConfirmRect() : pausePanelRect();
    if (uiCancelRequested(pauseCancelState_, input, ui, cancelPanel)) {
        leavePausePage();
        return;
    }

    if (pausePage_ == PauseMenuPage::QuitConfirm) {
        pauseConfirmSelection_ = 1;
        const UiRect quitRect = quitConfirmButtonRect(1);
        if (ui.pressed(quitRect)) {
            quitRequested_ = true;
            return;
        }
        if (input.confirmPressed() || input.useItemPressed()) {
            quitRequested_ = true;
            return;
        }
        ui.block(quitConfirmRect());
        return;
    }

    if (pausePage_ != PauseMenuPage::Main) {
        ui.block(pausePanelRect());
        return;
    }

    if (input.pressed(InputAction::MoveUp)) {
        pauseMenuSelection_ = (pauseMenuSelection_ + PauseMenuItemCount - 1) % PauseMenuItemCount;
    }
    if (input.pressed(InputAction::MoveDown)) {
        pauseMenuSelection_ = (pauseMenuSelection_ + 1) % PauseMenuItemCount;
    }
    for (int i = 0; i < PauseMenuItemCount; ++i) {
        const UiRect rect = pauseMenuItemRect(i);
        if (rect.contains(ui.mouse())) {
            pauseMenuSelection_ = i;
        }
        if (ui.pressed(rect)) {
            pauseMenuSelection_ = i;
            choosePauseMenuItem(i);
            return;
        }
    }
    if (input.confirmPressed() || input.useItemPressed()) {
        choosePauseMenuItem(pauseMenuSelection_);
        return;
    }

    ui.block(pausePanelRect());
}

void Game::updateGameOverScreen(const Input& input, UiContext& ui)
{
    if (input.pressed(InputAction::MoveUp)) {
        gameOverSelection_ = (gameOverSelection_ + GameOverItemCount - 1) % GameOverItemCount;
    }
    if (input.pressed(InputAction::MoveDown)) {
        gameOverSelection_ = (gameOverSelection_ + 1) % GameOverItemCount;
    }

    for (int i = 0; i < GameOverItemCount; ++i) {
        const UiRect rect = gameOverItemRect(i);
        if (rect.contains(ui.mouse())) {
            gameOverSelection_ = i;
        }
        if (ui.pressed(rect)) {
            gameOverSelection_ = i;
            if (i == 0) {
                retryAfterGameOver();
            } else {
                returnToBaseAfterGameOver();
            }
            return;
        }
    }

    if (input.confirmPressed() || input.useItemPressed()) {
        if (gameOverSelection_ == 0) {
            retryAfterGameOver();
        } else {
            returnToBaseAfterGameOver();
        }
        return;
    }

    ui.block(gameOverPanelRect());
}

void Game::updateStageClearScreen(const Input& input, UiContext& ui)
{
    if (input.pressed(InputAction::MoveUp)) {
        stageClearSelection_ = (stageClearSelection_ + StageClearItemCount - 1) % StageClearItemCount;
    }
    if (input.pressed(InputAction::MoveDown)) {
        stageClearSelection_ = (stageClearSelection_ + 1) % StageClearItemCount;
    }

    for (int i = 0; i < StageClearItemCount; ++i) {
        const UiRect rect = stageClearItemRect(i);
        if (rect.contains(ui.mouse())) {
            stageClearSelection_ = i;
        }
        if (ui.pressed(rect)) {
            stageClearSelection_ = i;
            returnToBaseFromNormalStage(true, false);
            return;
        }
    }

    if (input.confirmPressed() || input.useItemPressed()) {
        returnToBaseFromNormalStage(true, false);
        return;
    }

    ui.block(stageClearPanelRect());
}

int Game::unlockedRingHudCount() const
{
    return std::clamp(UnlockedRingCount, 0, SpellRingCount);
}

UiRect Game::ringStatusHudRect(int ringIndex, int unlockedRingCount) const
{
    const float screenHeight = static_cast<float>(camera_.height());
    const float totalHeight = static_cast<float>(unlockedRingCount) * RingStatusHudHeight +
        static_cast<float>(std::max(0, unlockedRingCount - 1)) * RingStatusHudGap;
    const float startY = std::max(
        TopInfoBarY + TopInfoBarHeight + 8.0f,
        screenHeight - RingStatusHudBottomMargin - totalHeight);
    return {{
        RingStatusHudLeftMargin,
        startY + static_cast<float>(ringIndex) * (RingStatusHudHeight + RingStatusHudGap),
    }, {RingStatusHudWidth, RingStatusHudHeight}};
}

void Game::updateRingStatusHud(UiContext& ui)
{
    const int unlockedRingCount = unlockedRingHudCount();
    for (int ringIndex = 0; ringIndex < unlockedRingCount; ++ringIndex) {
        if (!ui.pressed(ringStatusHudRect(ringIndex, unlockedRingCount))) {
            continue;
        }
        switchActiveRingWithLog(ringIndex - spellRing_.activeRingIndex());
        return;
    }
}

std::string Game::currentMapDisplayName() const
{
    if (basePresentationActive()) {
        return baseAreaName(baseArea_);
    }
    if (enemyTestActive_) {
        return "敵テスト";
    }
    if (!currentStageDefinition_.name.empty()) {
        return currentStageDefinition_.name;
    }
    return currentStageId_.empty() ? std::string{"Unknown Map"} : currentStageId_;
}

void Game::renderTopInfoBar(Renderer& renderer) const
{
    renderer.setScreenSpace();

    const float screenWidth = static_cast<float>(camera_.width());
    const float barWidth = std::max(1.0f, screenWidth - TopInfoBarX * 2.0f);
    renderer.fillRect({TopInfoBarX, TopInfoBarY}, {barWidth, TopInfoBarHeight}, {8, 10, 18, 210});
    renderer.drawRect({TopInfoBarX, TopInfoBarY}, {barWidth, TopInfoBarHeight}, {96, 104, 132, 170});

    const int textScale = 2;
    const Vec2 textMeasure = renderer.measureText("0", textScale);
    const float textY = TopInfoBarY + std::max(0.0f, (TopInfoBarHeight - textMeasure.y) * 0.5f) + 6.0f;
    InlineItemTextStyle moneyStyle;
    moneyStyle.text = {246, 230, 174, 255};
    moneyStyle.scale = textScale;
    moneyStyle.iconTextGap = TopInfoBarIconTextGap;
    moneyStyle.iconScale = TopInfoBarIconSize / std::max(1.0f, textMeasure.y);

    InlineItemTextStyle materialStyle = moneyStyle;
    materialStyle.text = {232, 236, 244, 255};

    const std::string moneyEntry = inlineWorldIconTag(worldIconKey(WorldIconId::MoneyLarge)) + std::to_string(money_) + "G";
    const Vec2 moneyEntrySize = measureInlineItemText(renderer, moneyEntry, moneyStyle);
    constexpr std::array<TopInfoMaterial, 4> Materials{{
        {MaterialType::OldWoodBuildingMaterial},
        {MaterialType::EnhancementOre},
        {MaterialType::MoonFragment},
        {MaterialType::ManaDrop},
    }};

    std::array<std::string, Materials.size()> materialEntries{};
    std::array<Vec2, Materials.size()> materialEntrySizes{};
    float rightGroupWidth = moneyEntrySize.x;
    for (std::size_t i = 0; i < Materials.size(); ++i) {
        materialEntries[i] = inlineMaterialIconTag(Materials[i].type) + std::to_string(inventory_.materialCount(Materials[i].type));
        materialEntrySizes[i] = measureInlineItemText(renderer, materialEntries[i], materialStyle);
        rightGroupWidth += TopInfoBarGroupGap + materialEntrySizes[i].x;
    }

    const float rightEdge = TopInfoBarX + barWidth - TopInfoBarPaddingX;
    float rightX = rightEdge - rightGroupWidth;
    rightX = std::max(TopInfoBarX + TopInfoBarPaddingX, rightX);

    const float mapX = TopInfoBarX + TopInfoBarPaddingX;
    const float mapMaxWidth = std::max(0.0f, rightX - mapX - 18.0f);
    const std::string mapName = fittedSingleLineText(renderer, currentMapDisplayName(), mapMaxWidth, textScale);
    renderer.drawText({mapX, textY}, mapName, {246, 246, 252, 255}, textScale);

    float x = rightX;
    drawInlineItemText(renderer, objectCatalog_, {x, textY}, moneyEntry, moneyStyle);
    x += moneyEntrySize.x;

    for (std::size_t i = 0; i < Materials.size(); ++i) {
        x += TopInfoBarGroupGap;
        drawInlineItemText(renderer, objectCatalog_, {x, textY}, materialEntries[i], materialStyle);
        x += materialEntrySizes[i].x;
    }
}

void Game::renderOpeningKamishibai(Renderer& renderer) const
{
    openingRenderer_.render(renderer, openingPlayer_, camera_.width(), camera_.height());
}

void Game::renderTitleScreen(Renderer& renderer) const
{
    openingRenderer_.renderTitleScreen(renderer, openingTitleImagePath(openingPages_), camera_.width(), camera_.height());
}

void Game::renderScreenTransitionOverlay(Renderer& renderer)
{
    if (!screenTransition_.active()) {
        renderer.destroyFrameSnapshot(screenTransitionSnapshot_);
        return;
    }

    if (screenTransition_.phase == ScreenTransitionPhase::CrossFadeCapture) {
        renderer.destroyFrameSnapshot(screenTransitionSnapshot_);
        screenTransitionSnapshot_ = renderer.captureFrameSnapshot();
        if (!screenTransition_.applied) {
            applyScreenTransitionTarget(screenTransition_.target);
            screenTransition_.applied = true;
        }
        screenTransition_.elapsed = 0.0f;
        screenTransition_.phase = ScreenTransitionPhase::CrossFading;
    }

    float alpha = 0.0f;
    switch (screenTransition_.phase) {
    case ScreenTransitionPhase::Idle:
        break;
    case ScreenTransitionPhase::CrossFadeCapture:
        break;
    case ScreenTransitionPhase::CrossFading:
        alpha = 1.0f - smoothStep01(screenTransition_.elapsed / std::max(0.001f, ScreenTransitionCrossFadeSeconds));
        if (alpha > 0.0f) {
            renderer.setScreenSpace();
            renderer.drawFrameSnapshot(
                screenTransitionSnapshot_,
                {0.0f, 0.0f},
                {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())},
                {255, 255, 255, alphaByte(255.0f * alpha)});
        }
        return;
    case ScreenTransitionPhase::FadingOut:
        alpha = smoothStep01(screenTransition_.elapsed / std::max(0.001f, ScreenTransitionFadeOutSeconds));
        break;
    case ScreenTransitionPhase::HoldBlack:
        alpha = 1.0f;
        break;
    case ScreenTransitionPhase::FadingIn:
        alpha = 1.0f - smoothStep01(screenTransition_.elapsed / std::max(0.001f, ScreenTransitionFadeInSeconds));
        break;
    }

    if (alpha <= 0.0f) {
        return;
    }

    renderer.setScreenSpace();
    renderer.fillRect(
        {0.0f, 0.0f},
        {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())},
        {0, 0, 0, alphaByte(255.0f * alpha)});
}

void Game::renderDungeonStatusHud(Renderer& renderer) const
{
    renderer.setScreenSpace();

    const float screenWidth = static_cast<float>(camera_.width());
    const float screenHeight = static_cast<float>(camera_.height());
    const UiRect panel{{
        std::max(8.0f, screenWidth - DungeonStatusHudRightMargin - DungeonStatusHudWidth),
        std::max(TopInfoBarY + TopInfoBarHeight + 8.0f, screenHeight - DungeonStatusHudBottomMargin - DungeonStatusHudHeight),
    }, {DungeonStatusHudWidth, DungeonStatusHudHeight}};

    drawUiSubPanel(renderer, panel);

    const Vec2 content = panel.pos + Vec2{DungeonStatusHudPadding, DungeonStatusHudPadding};
    constexpr int TextScale = 2;
    char buffer[64];

    renderer.drawText(content, "STATUS", {246, 246, 252, 255}, TextScale);

    const int hpMax = std::max(1, player_.maxHp);
    const int hp = std::clamp(player_.hp, 0, hpMax);
    std::snprintf(buffer, sizeof(buffer), "HP %02d/%02d", hp, hpMax);
    renderer.drawText(content + Vec2{0.0f, 28.0f}, buffer, {255, 232, 232, 255}, TextScale);

    const float barWidth = panel.size.x - DungeonStatusHudPadding * 2.0f;
    const Vec2 hpBarPos = content + Vec2{0.0f, 52.0f};
    UiGaugeStyle hpGaugeStyle;
    hpGaugeStyle.fill.start = {224, 74, 84, 255};
    hpGaugeStyle.fill.end = {255, 126, 116, 255};
    hpGaugeStyle.track = {42, 18, 24, 230};
    hpGaugeStyle.trackInner = {58, 24, 32, 220};
    hpGaugeStyle.trackOuter = {255, 220, 224, 82};
    hpGaugeStyle.shadow = {0, 0, 0, 90};
    hpGaugeStyle.highlight = {255, 244, 244, 92};
    hpGaugeStyle.capGlow = {255, 116, 128, 58};
    hpGaugeStyle.capCore = {255, 244, 244, 210};
    hpGaugeStyle.trackInnerInset = 4.0f;
    hpGaugeStyle.shadowOffsetY = 2.0f;
    hpGaugeStyle.shadowExtra = 5.0f;
    drawUiGauge(
        renderer,
        {hpBarPos, {barWidth, DungeonStatusHudBarHeight}},
        static_cast<float>(hp) / static_cast<float>(hpMax),
        hpGaugeStyle);

    std::snprintf(buffer, sizeof(buffer), "Lv %02d", std::max(1, player_.level));
    renderer.drawText(content + Vec2{0.0f, 70.0f}, buffer, {232, 236, 244, 255}, TextScale);

    const int xpToNext = std::max(1, player_.xpToNext);
    const int xp = std::clamp(player_.xp, 0, xpToNext);
    std::snprintf(buffer, sizeof(buffer), "EXP %02d/%02d", xp, xpToNext);
    renderer.drawText(content + Vec2{0.0f, 94.0f}, buffer, {222, 236, 255, 255}, TextScale);
}

void Game::renderRingStatusHud(Renderer& renderer) const
{
    renderer.setScreenSpace();

    const int unlockedRingCount = unlockedRingHudCount();

    char buffer[96];
    for (int ringIndex = 0; ringIndex < unlockedRingCount; ++ringIndex) {
        const UiRect panel = ringStatusHudRect(ringIndex, unlockedRingCount);
        const bool active = ringIndex == spellRing_.activeRingIndex();

        drawUiSubPanel(renderer, panel);
        if (active) {
            renderer.drawRect(panel.pos, panel.size, {255, 236, 158, 255});
            renderer.drawRect(panel.pos + Vec2{1.0f, 1.0f}, panel.size - Vec2{2.0f, 2.0f}, {255, 236, 158, 190});
        }

        const Vec2 content = panel.pos + Vec2{RingStatusHudPadding, RingStatusHudPadding};
        const auto& items = spellRing_.itemsForRing(ringIndex);
        float ringWeight = 0.0f;
        for (const SpellRingItem& item : items) {
            ringWeight += std::max(0.0f, item.weight);
        }

        std::snprintf(buffer, sizeof(buffer), "%sRing %d", active ? "> " : "", ringIndex + 1);
        renderer.drawText(content, buffer, active ? Color{255, 236, 158, 255} : ui::Text, 2);

        std::snprintf(buffer, sizeof(buffer), "アイテム数 %02d / %02d", static_cast<int>(items.size()), spellRing_.maxItemCount());
        renderer.drawText(content + Vec2{0.0f, 24.0f}, buffer, {232, 236, 244, 255}, 2);

        std::snprintf(buffer, sizeof(buffer), "重量 %.1f / %.1f", ringWeight, spellRing_.maxEquippedWeight());
        renderer.drawText(content + Vec2{0.0f, 48.0f}, buffer, {222, 236, 255, 255}, 2);
    }
}

void Game::renderDungeonLogs(Renderer& renderer) const
{
    if (dungeonLogs_.empty()) {
        return;
    }

    renderer.setScreenSpace();

    const float screenWidth = static_cast<float>(camera_.width());
    const float screenHeight = static_cast<float>(camera_.height());
    const float topLimit = TopInfoBarY + TopInfoBarHeight + 8.0f;
    const float statusTopY = screenHeight - DungeonStatusHudBottomMargin - DungeonStatusHudHeight;
    const float maxBottomY = std::max(topLimit + DungeonLogRowHeight, statusTopY - DungeonLogStatusGap);

    int visibleCount = std::min(static_cast<int>(dungeonLogs_.size()), DungeonLogMaxVisible);
    const auto blockHeight = [](int count) {
        return static_cast<float>(count) * DungeonLogRowHeight +
            static_cast<float>(std::max(0, count - 1)) * DungeonLogGap;
    };
    while (visibleCount > 0 && blockHeight(visibleCount) > maxBottomY - topLimit) {
        --visibleCount;
    }
    if (visibleCount <= 0) {
        return;
    }

    const float totalHeight = blockHeight(visibleCount);
    const float x = std::max(8.0f, screenWidth - DungeonLogRightMargin - DungeonLogWidth);
    const float y = std::clamp(screenHeight * DungeonLogTargetYRatio, topLimit, maxBottomY - totalHeight);
    const int firstIndex = static_cast<int>(dungeonLogs_.size()) - visibleCount;

    constexpr int TextScale = 2;
    const Vec2 textMeasure = renderer.measureText("0", TextScale);
    for (int i = 0; i < visibleCount; ++i) {
        const DungeonLogEntry& entry = dungeonLogs_[static_cast<std::size_t>(firstIndex + i)];
        const float rowY = y + static_cast<float>(i) * (DungeonLogRowHeight + DungeonLogGap);
        const float fadeDenom = std::max(0.01f, entry.lifetime - DungeonLogFadeStart);
        const float fade = entry.age <= DungeonLogFadeStart
            ? 1.0f
            : std::clamp(1.0f - (entry.age - DungeonLogFadeStart) / fadeDenom, 0.0f, 1.0f);

        const unsigned char rightAlpha = static_cast<unsigned char>(std::clamp(210.0f * fade, 0.0f, 210.0f));
        renderer.fillGradientRect(
            {x, rowY},
            {DungeonLogWidth, DungeonLogRowHeight},
            {0, 0, 0, 0},
            {0, 0, 0, rightAlpha},
            GradientDirection::LeftToRight);

        std::string message = entry.message;
        if (entry.count > 0) {
            message = entry.label + " x" + std::to_string(entry.count) + entry.suffix;
        }
        const unsigned char textAlpha = static_cast<unsigned char>(std::clamp(255.0f * fade, 0.0f, 255.0f));
        InlineItemTextStyle textStyle;
        textStyle.text = {246, 246, 252, textAlpha};
        textStyle.scale = TextScale;
        textStyle.iconTextGap = 4.0f;
        textStyle.iconScale = TopInfoBarIconSize / std::max(1.0f, textMeasure.y);
        textStyle.outlineEnabled = true;
        textStyle.outline = {0, 0, 0, textAlpha};
        textStyle.outlinePx = 2;
        message = fittedInlineItemText(renderer, std::move(message), DungeonLogWidth - 26.0f, textStyle);
        const Vec2 textPos{
            x + DungeonLogWidth - 14.0f,
            rowY + std::max(0.0f, (DungeonLogRowHeight - textMeasure.y) * 0.5f) + 6.0f,
        };
        drawInlineItemTextRightAligned(renderer, objectCatalog_, textPos, message, textStyle);
    }
}

void Game::renderWarpReturnUi(Renderer& renderer) const
{
    if (mode_ != ScreenMode::Playing || enemyTestActive_ || !warpPointsEnabled_) {
        return;
    }

    renderer.setScreenSpace();
    const float screenWidth = static_cast<float>(camera_.width());
    const float screenHeight = static_cast<float>(camera_.height());

    if (warpReturnConfirmActive_) {
        renderer.fillRect({0.0f, 0.0f}, {screenWidth, screenHeight}, {0, 0, 0, 118});
        const UiRect panel = warpReturnConfirmRect();
        UiWindowScope confirmWindow(renderer, "warp.return_confirm", panel, "", "F/Enter 決定  Esc/右クリック 戻る", UiWindowOptions{true, true});

        const std::string title = "拠点へ帰還しますか？";
        const Vec2 titleSize = renderer.measureText(title, 3);
        renderer.drawText(
            {panel.pos.x + (panel.size.x - titleSize.x) * 0.5f, panel.pos.y + 66.0f},
            title,
            ui::Text,
            3);

        constexpr float BodyWidth = 330.0f;
        const Vec2 bodyPos{panel.pos.x + (panel.size.x - BodyWidth) * 0.5f, panel.pos.y + 112.0f};
        renderer.drawWrappedText(
            bodyPos,
            "現在のダンジョン状態を保持したまま、ダンジョン入口へ戻ります。",
            BodyWidth,
            ui::TextMuted,
            2);

        drawUiButton(
            renderer,
            warpReturnConfirmButtonRect(0),
            "帰還する",
            warpReturnConfirmSelection_ == 0,
            uiActionButtonStyle());
        drawUiButton(
            renderer,
            warpReturnConfirmButtonRect(1),
            "戻る",
            warpReturnConfirmSelection_ == 1,
            uiCancelButtonStyle());
        return;
    }

    if (focusedWarpReturnPointIndex_ == DungeonEntranceReturnFocusIndex) {
        const std::string prompt = "ダンジョン入口  F/Enter 拠点へ帰還";
        constexpr int TextScale = 2;
        const Vec2 textSize = renderer.measureText(prompt, TextScale);
        const Vec2 padding{22.0f, 10.0f};
        const Vec2 size{textSize.x + padding.x * 2.0f, textSize.y + padding.y * 2.0f};
        const Vec2 pos{
            (screenWidth - size.x) * 0.5f,
            std::max(96.0f, screenHeight - 174.0f),
        };
        renderer.fillRect(pos, size, {12, 18, 34, 218});
        renderer.drawRect(pos, size, {154, 230, 246, 210});
        renderer.drawText(pos + padding + Vec2{1.0f, 1.0f}, prompt, {0, 0, 0, 180}, TextScale);
        renderer.drawText(pos + padding, prompt, {232, 250, 255, 255}, TextScale);
        return;
    }

    if (focusedWarpReturnPointIndex_ < 0 ||
        focusedWarpReturnPointIndex_ >= static_cast<int>(warpPoints_.size())) {
        return;
    }

    const WarpPoint& point = warpPoints_[static_cast<std::size_t>(focusedWarpReturnPointIndex_)];
    if (!point.discovered) {
        return;
    }

    char buffer[96];
    std::snprintf(buffer, sizeof(buffer), "ワープポイント %d  F/Enter 拠点へ帰還", point.index + 1);
    constexpr int TextScale = 2;
    const Vec2 textSize = renderer.measureText(buffer, TextScale);
    const Vec2 padding{22.0f, 10.0f};
    const Vec2 size{textSize.x + padding.x * 2.0f, textSize.y + padding.y * 2.0f};
    const Vec2 pos{
        (screenWidth - size.x) * 0.5f,
        std::max(96.0f, screenHeight - 174.0f),
    };
    renderer.fillRect(pos, size, {12, 18, 34, 218});
    renderer.drawRect(pos, size, {154, 230, 246, 210});
    renderer.drawText(pos + padding + Vec2{1.0f, 1.0f}, buffer, {0, 0, 0, 180}, TextScale);
    renderer.drawText(pos + padding, buffer, {232, 250, 255, 255}, TextScale);
}

void Game::renderWorldLoadingScreen(Renderer& renderer, float totalSeconds) const
{
    if (mode_ != ScreenMode::WorldLoading) {
        return;
    }

    renderer.setScreenSpace();
    const float screenW = static_cast<float>(camera_.width());
    const float screenH = static_cast<float>(camera_.height());

    constexpr float BarW = 280.0f;
    constexpr float TrackH = 16.0f;
    constexpr float MarginRight = 28.0f;
    constexpr float MarginBottom = 28.0f;
    const float barLeft = std::max(12.0f, screenW - MarginRight - BarW);
    const float barCenterY = std::max(20.0f, screenH - MarginBottom - TrackH * 0.5f);
    const UiRect bar{{barLeft, barCenterY - TrackH * 0.5f}, {BarW, TrackH}};
    const float progress = std::clamp(worldBuildProgress(), 0.0f, 1.0f);
    const float pulse = 0.76f + 0.24f * std::sin(totalSeconds * 5.0f);
    UiGaugeStyle loadingGaugeStyle;
    loadingGaugeStyle.fill.start = {static_cast<unsigned char>(108.0f + 48.0f * pulse), 206, 236, 230};
    loadingGaugeStyle.fill.end = {132, 230, 250, 230};
    loadingGaugeStyle.track = {12, 16, 24, 190};
    loadingGaugeStyle.trackInner = {30, 38, 52, 220};
    loadingGaugeStyle.trackOuter = {218, 228, 244, 78};
    loadingGaugeStyle.shadow = {0, 0, 0, 105};
    loadingGaugeStyle.tick = {255, 255, 255, 32};
    loadingGaugeStyle.highlight = {255, 255, 255, 118};
    loadingGaugeStyle.capGlow = {132, 230, 250, 78};
    loadingGaugeStyle.capCore = {246, 252, 255, 225};
    loadingGaugeStyle.tickCount = 8;
    loadingGaugeStyle.shimmer = {255, 255, 255, 76};
    loadingGaugeStyle.shimmerPhase =
        std::fmod(std::max(0.0f, totalSeconds) * 116.0f, BarW + loadingGaugeStyle.shimmerWidth) /
        (BarW + loadingGaugeStyle.shimmerWidth);
    drawUiGauge(renderer, bar, progress, loadingGaugeStyle);

    const std::string label = "LOADING";
    const Vec2 labelSize = renderer.measureText(label, 2);
    const Vec2 labelPos{bar.pos.x + bar.size.x - labelSize.x, bar.pos.y - labelSize.y - 9.0f};
    renderer.drawText(labelPos + Vec2{1.0f, 1.0f}, label, {0, 0, 0, 170}, 2);
    renderer.drawText(labelPos, label, {246, 246, 252, 230}, 2);
}

void Game::renderRingScreen(Renderer& renderer, float totalTime) const
{
    if (mode_ != ScreenMode::Ring) {
        return;
    }

    renderer.setScreenSpace();
    const UiRect panel = ringPanelRect();
    UiCancelControlScope cancelScope(ringCancelState_);
    constexpr std::string_view RingHelpText = UnlockedRingCount > 1
        ? "Z/X・1-3 リング選択  F/Enter 確定/詳細  Q/E アイテム選択  A/D・←/→ 位置  R 外す  G つかむ/置く  P 保護  Esc/右クリック 戻る"
        : "Q/E アイテム選択  A/D・←/→ 位置  F/Enter 詳細  R 外す  G つかむ/置く  P 保護  Esc/右クリック 戻る";
    UiWindowScope ringWindow(renderer, "ring.manage", panel, "リング", RingHelpText, UiWindowOptions{true, true});

    char buffer[192];
    std::array<UiTabItem, UnlockedRingCount> ringTabs{};
    std::array<UiRect, UnlockedRingCount> ringTabRects{};
    std::array<std::string, UnlockedRingCount> ringTabLabels{};
    for (int i = 0; i < UnlockedRingCount; ++i) {
        ringTabLabels[static_cast<std::size_t>(i)] = "リング " + std::to_string(i + 1);
        ringTabs[static_cast<std::size_t>(i)] = {ringTabLabels[static_cast<std::size_t>(i)], true};
        ringTabRects[static_cast<std::size_t>(i)] = ringTabRect(i);
    }
    drawUiTabs(
        renderer,
        ringTabs_,
        spellRing_.activeRingIndex(),
        ringTabs.data(),
        static_cast<int>(ringTabs.size()),
        ringTabRects.data());
    drawUiButton(renderer, ringArrangeButtonRect(), "整列", false, uiActionButtonStyle());

    const bool actualRing = true;
    const auto& items = spellRing_.items();
    (void)actualRing;
    const RingShape activeShape = spellRing_.activeRingShape();

    std::snprintf(buffer, sizeof(buffer), "形状 %s   装着 %02d/%02d   半径 %.0f   速度 %.2f   重量 %.1f/%.1fkg   重量補正 %.2f",
        ringShapeDisplayName(activeShape),
        static_cast<int>(items.size()),
        spellRing_.maxItemCount(),
        spellRing_.radius(),
        spellRing_.effectiveAngularSpeed(),
        spellRing_.totalEquippedWeight(),
        spellRing_.maxEquippedWeight(),
        spellRing_.weightSpeedMultiplier());
    renderer.drawText(panel.pos + Vec2{48.0f, 520.0f}, buffer, {202, 206, 216, 255}, 2);

    renderer.drawText(panel.pos + Vec2{48.0f, 160.0f}, "リング配置", {246, 248, 255, 255}, 3);
    const Vec2 orbitCenter = ringUiOrbitCenter(spellRing_);
    std::vector<Vec2> orbitPath = getRingPathSamplePoints(
        spellRing_.center(),
        ringUiOrbitContext(spellRing_, balance_, 0, 1),
        160);
    for (Vec2& point : orbitPath) {
        point = applyRingUiShapeRotation(spellRing_, ringWorldToUi(spellRing_, point));
    }
    drawMagicOrbitPath(
        renderer,
        orbitPath,
        orbitCenter,
        MagicOrbitDrawOptions{
            activeShape,
            true,
            false,
            true,
            true,
            spellRing_.activeRingIndex(),
            totalTime,
            1.0f,
        });
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        const SpellRingItem& item = items[static_cast<std::size_t>(i)];
        float displayAngle = item.localAngle;
        if (i == ringDragItemIndex_ && (ringDragActive_ || ringSnapActive_)) {
            displayAngle = ringDragDisplayAngle_;
        }
        const Vec2 itemAnchor = ringItemUiCenterAtAngle(
            displayAngle,
            spellRing_,
            balance_,
            i,
            static_cast<int>(items.size()));
        SpellRingItem displayItem = item;
        displayItem.worldPosition = itemAnchor;
        const Vec2 itemCenter = ringItemDrawPosition(displayItem, totalTime);
        const Vec2 outward = normalize(itemAnchor - orbitCenter);
        const bool selected = i == ringSlotSelection_;
        const bool invalidDragPosition = selected && ringDragActive_ && !spellRing_.canPlaceItemAtAngle(i, displayAngle);
        const ItemData* object = objectForRingItem(objectCatalog_, item);
        if (activeShape != RingShape::FigureEight) {
            const Color angleLineColor = selected ? Color{255, 230, 150, 120} : Color{94, 102, 128, 85};
            const Vec2 radial = itemAnchor - orbitCenter;
            Vec2 tangent = normalize(Vec2{-radial.y, radial.x});
            if (lengthSquared(tangent) <= 0.0001f) {
                tangent = {0.0f, 1.0f};
            }
            constexpr float AngleLineHalfWidthPx = 0.5f;
            renderer.drawLine(orbitCenter + tangent * AngleLineHalfWidthPx, itemAnchor + tangent * AngleLineHalfWidthPx, angleLineColor);
            renderer.drawLine(orbitCenter - tangent * AngleLineHalfWidthPx, itemAnchor - tangent * AngleLineHalfWidthPx, angleLineColor);
        }
        drawRingItemShape(renderer, item, object, itemCenter, outward, selected, invalidDragPosition);
        std::snprintf(buffer, sizeof(buffer), "%d", i + 1);
        renderer.drawText(itemCenter + Vec2{-5.0f, 22.0f}, buffer, selected ? Color{255, 230, 150, 255} : Color{174, 182, 198, 255}, 1);
    }

    std::string ringDetailTitle = "空き";
    if (ringSlotSelection_ < static_cast<int>(items.size())) {
        ringDetailTitle = ringItemDisplayName(objectCatalog_, items[ringSlotSelection_]);
    }

    const UiRect ringDetailPanel = ringDetailRect();
    drawUiSubPanel(renderer, ringDetailPanel);
    float detailLineY = drawUiDetailHeader(renderer, ringDetailPanel, ringDetailTitle);

    if (ringSlotSelection_ < static_cast<int>(items.size())) {
        const SpellRingItem& item = items[ringSlotSelection_];
        const ItemData* object = objectForRingItem(objectCatalog_, item);
        const std::vector<std::string> effectLines = object != nullptr
            ? encyclopedia_.getObjectEffectDisplayLines(object->id, objectCatalog_, EffectRevealMode::WithUnknown)
            : std::vector<std::string>{};
        const std::string effectText = joinEffectLines(effectLines);
        drawUiDetailText(renderer, ringDetailPanel, detailLineY, object != nullptr && !object->description.empty() ? object->description : "-");
        std::snprintf(buffer, sizeof(buffer), "%d", static_cast<int>(effectLines.size()));
        drawUiDetailLine(renderer, ringDetailPanel, detailLineY, "効果数", buffer);
        drawUiDetailText(renderer, ringDetailPanel, detailLineY, "効果");
        drawUiDetailText(renderer, ringDetailPanel, detailLineY, effectText);
        drawUiDetailLine(renderer, ringDetailPanel, detailLineY, "形状", ringShapeDisplayName(activeShape));
        if (item.durability < 0) {
            std::snprintf(buffer, sizeof(buffer), "壊れない");
        } else {
            std::snprintf(buffer, sizeof(buffer), "%d/%d", item.durability, item.maxDurability);
        }
        drawUiDetailLine(renderer, ringDetailPanel, detailLineY, "耐久力", buffer);
        std::snprintf(buffer, sizeof(buffer), "%.1fkg", item.weight);
        drawUiDetailLine(renderer, ringDetailPanel, detailLineY, "重さ", buffer);
    } else {
        drawUiDetailText(renderer, ringDetailPanel, detailLineY, "アイテム未配置");
        drawUiDetailText(renderer, ringDetailPanel, detailLineY, "-");
        drawUiDetailLine(renderer, ringDetailPanel, detailLineY, "効果数", "0");
        drawUiDetailText(renderer, ringDetailPanel, detailLineY, "効果");
        drawUiDetailText(renderer, ringDetailPanel, detailLineY, "-");
        drawUiDetailLine(renderer, ringDetailPanel, detailLineY, "形状", ringShapeDisplayName(activeShape));
        drawUiDetailLine(renderer, ringDetailPanel, detailLineY, "耐久力", "-");
        drawUiDetailLine(renderer, ringDetailPanel, detailLineY, "重さ", "-");
    }

    if (ringGrabActive_) {
        const std::string grabbedName = ringItemDisplayName(objectCatalog_, ringGrabbedItem_);
        std::snprintf(buffer, sizeof(buffer), "つかみ中: %s   G または F で置く / Esc でキャンセル", grabbedName.c_str());
        renderer.drawText(panel.pos + Vec2{48.0f, 556.0f}, buffer, {255, 230, 150, 255}, 2);
    } else if (!ringStatus_.empty()) {
        renderer.drawText(panel.pos + Vec2{48.0f, 556.0f}, ringStatus_, {255, 230, 150, 255}, 2);
    }

    const int commandItemIndex = ringCommandItemIndex_ >= 0 ? ringCommandItemIndex_ : ringSlotSelection_;
    const bool commandCanRemove = commandItemIndex >= 0 &&
        commandItemIndex < static_cast<int>(items.size()) &&
        !items[static_cast<std::size_t>(commandItemIndex)].objectId.empty();
    const std::array<UiCommandMenuItem, 1> commandItems = ringCommandItems(commandCanRemove);
    drawUiCommandMenu(renderer, ringCommandMenu_, commandItems.data(), static_cast<int>(commandItems.size()));
}

void Game::renderPauseMenu(Renderer& renderer) const
{
    if (mode_ != ScreenMode::PauseMenu) {
        return;
    }

    renderer.setScreenSpace();
    const UiRect panel = pausePanelRect();
    UiCancelControlScope cancelScope(pauseCancelState_);
    const char* pauseTitle = pausePage_ == PauseMenuPage::Status ? "ステータス" : "PAUSED";
    const char* pauseHelp = pausePage_ == PauseMenuPage::Status
        ? "Esc/右クリック 戻る"
        : (pausePage_ == PauseMenuPage::Ring ? "Z/X でアクティブリング切替  Esc/右クリック 戻る" : "F/Enter 決定  Esc/右クリック 戻る");
    UiWindowScope pauseWindow(
        renderer,
        "pause.main",
        panel,
        pauseTitle,
        pauseHelp,
        UiWindowOptions{true, pausePage_ != PauseMenuPage::QuitConfirm});

    char buffer[160];
    if (pausePage_ == PauseMenuPage::Main) {
        for (int i = 0; i < PauseMenuItemCount; ++i) {
            const bool selected = i == pauseMenuSelection_;
            drawUiButton(renderer, pauseMenuItemRect(i), pauseMenuItemName(i), selected);
        }
        return;
    }

    if (pausePage_ == PauseMenuPage::Status) {
        const Vec2 labelPos = panel.pos + Vec2{74.0f, 110.0f};
        const Vec2 valuePos = panel.pos + Vec2{236.0f, 110.0f};
        constexpr float RowHeight = 32.0f;
        const auto drawStatusRow = [&](int row, std::string_view label, std::string_view value) {
            const float y = static_cast<float>(row) * RowHeight;
            renderer.drawText(labelPos + Vec2{0.0f, y}, label, ui::TextMuted, 2);
            renderer.drawText(valuePos + Vec2{0.0f, y}, value, {230, 230, 236, 255}, 2);
        };

        std::snprintf(buffer, sizeof(buffer), "%02d", player_.hp);
        drawStatusRow(0, "HP", buffer);
        std::snprintf(buffer, sizeof(buffer), "%02d", player_.level);
        drawStatusRow(1, "Level", buffer);
        std::snprintf(buffer, sizeof(buffer), "%02d/%02d", player_.xp, player_.xpToNext);
        drawStatusRow(2, "XP", buffer);
        std::snprintf(buffer, sizeof(buffer), "%d", spellRing_.activeRingIndex() + 1);
        drawStatusRow(3, "Ring", buffer);
        std::snprintf(buffer, sizeof(buffer), "%02d/%02d", static_cast<int>(spellRing_.items().size()), spellRing_.maxItemCount());
        drawStatusRow(4, "Items", buffer);
        std::snprintf(buffer, sizeof(buffer), "%.0f", spellRing_.radius());
        drawStatusRow(5, "Radius", buffer);
        std::snprintf(buffer, sizeof(buffer), "%.1f", spellRing_.effectiveAngularSpeed());
        drawStatusRow(6, "Speed", buffer);
    } else if (pausePage_ == PauseMenuPage::Items) {
        renderer.drawText(panel.pos + Vec2{48.0f, 102.0f}, "アイテム", {246, 235, 255, 255}, 3);
        renderer.drawText(panel.pos + Vec2{58.0f, 164.0f}, "通常画面のショートカットHUDとアイテム画面で管理します。", {230, 230, 236, 255}, 2);
        renderer.drawText(panel.pos + Vec2{58.0f, 206.0f}, "I キーでアイテム画面を開けます。", {198, 198, 206, 255}, 2);
    } else if (pausePage_ == PauseMenuPage::Ring) {
        renderer.drawText(panel.pos + Vec2{48.0f, 102.0f}, "リング", {246, 235, 255, 255}, 3);
        std::snprintf(buffer, sizeof(buffer), "アクティブリング %d", spellRing_.activeRingIndex() + 1);
        renderer.drawText(panel.pos + Vec2{58.0f, 164.0f}, buffer, {230, 230, 236, 255}, 2);
        std::snprintf(buffer, sizeof(buffer), "装着アイテム %02d/%02d", static_cast<int>(spellRing_.items().size()), spellRing_.maxItemCount());
        renderer.drawText(panel.pos + Vec2{58.0f, 206.0f}, buffer, {230, 230, 236, 255}, 2);
    } else if (pausePage_ == PauseMenuPage::Options) {
        renderer.drawText(panel.pos + Vec2{48.0f, 102.0f}, "オプション", {246, 235, 255, 255}, 3);
        renderer.drawText(panel.pos + Vec2{58.0f, 164.0f}, "仮画面です。設定項目は後続実装で追加します。", {230, 230, 236, 255}, 2);
    } else if (pausePage_ == PauseMenuPage::QuitConfirm) {
        const UiRect confirm = quitConfirmRect();
        UiWindowScope confirmWindow(renderer, "pause.quit_confirm", confirm, "確認", "Esc/右クリック 戻る", UiWindowOptions{true, true});
        renderer.drawText(confirm.pos + Vec2{58.0f, 82.0f}, "ゲームを終了しますか？", ui::Text, 3);
        renderer.drawText(confirm.pos + Vec2{62.0f, 126.0f}, "セーブは拠点でのみ実行できます。", ui::TextMuted, 2);
        drawUiButton(renderer, quitConfirmButtonRect(1), "終了する", true, uiActionButtonStyle());
        return;
    }
}

void Game::renderGameOverScreen(Renderer& renderer) const
{
    if (mode_ != ScreenMode::GameOver) {
        return;
    }

    renderer.setScreenSpace();
    renderer.fillRect({0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}, {0, 0, 0, 150});
    const UiRect panel = gameOverPanelRect();
    UiWindowScope gameOverWindow(renderer, "game_over", panel, "GAME OVER", "F/Enter 決定");
    renderer.drawText(panel.pos + Vec2{118.0f, 92.0f}, "リザルト", ui::Text, 3);

    char buffer[160];
    const std::string deathCause = playerDeathCauseText(player_);
    std::snprintf(buffer, sizeof(buffer), "死因      %s", deathCause.c_str());
    renderer.drawText(panel.pos + Vec2{136.0f, 130.0f}, buffer, {255, 214, 220, 255}, 2);
    std::snprintf(buffer, sizeof(buffer), "到達時間  %s", formatRunTime(runStats_.elapsedSeconds).c_str());
    renderer.drawText(panel.pos + Vec2{136.0f, 168.0f}, buffer, {230, 230, 236, 255}, 2);
    std::snprintf(buffer, sizeof(buffer), "レベル    %d", player_.level);
    renderer.drawText(panel.pos + Vec2{136.0f, 206.0f}, buffer, {230, 230, 236, 255}, 2);
    std::snprintf(buffer, sizeof(buffer), "撃破数    %d", runStats_.defeatedEnemies);
    renderer.drawText(panel.pos + Vec2{136.0f, 244.0f}, buffer, {230, 230, 236, 255}, 2);
    std::snprintf(buffer, sizeof(buffer), "掘削数    %d", runStats_.dugTiles);
    renderer.drawText(panel.pos + Vec2{136.0f, 282.0f}, buffer, {230, 230, 236, 255}, 2);
    std::snprintf(buffer, sizeof(buffer), "入手アイテム数  %d", runStats_.acquiredItems);
    renderer.drawText(panel.pos + Vec2{136.0f, 320.0f}, buffer, {230, 230, 236, 255}, 2);

    for (int i = 0; i < GameOverItemCount; ++i) {
        drawUiButton(renderer, gameOverItemRect(i), gameOverItemName(i), i == gameOverSelection_, i == 0 ? uiActionButtonStyle() : uiCancelButtonStyle());
    }
    if (!gameOverStatus_.empty()) {
        renderer.drawText(panel.pos + Vec2{152.0f, 474.0f}, gameOverStatus_, {255, 230, 150, 255}, 2);
    }
}

void Game::renderStageClearScreen(Renderer& renderer) const
{
    if (mode_ != ScreenMode::StageClear) {
        return;
    }

    renderer.setScreenSpace();
    renderer.fillRect({0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}, {0, 0, 0, 130});
    const UiRect panel = stageClearPanelRect();
    UiWindowScope stageClearWindow(renderer, "stage_clear", panel, "STAGE CLEAR", "F/Enter 決定");
    renderer.drawText(panel.pos + Vec2{118.0f, 92.0f}, "シナリオ", ui::Text, 3);

    char buffer[160];
    std::snprintf(buffer, sizeof(buffer), "深部の主を退け、落ちた星のかけらが静かに光を取り戻した。");
    renderer.drawText(panel.pos + Vec2{136.0f, 148.0f}, buffer, {230, 238, 232, 255}, 2);
    renderer.drawText(panel.pos + Vec2{136.0f, 186.0f}, "村の魔女たちは、さらに深い層へ続く道を見つける。", {230, 238, 232, 255}, 2);
    std::snprintf(buffer, sizeof(buffer), "次ステージ解禁: Stage %d", unlockedStages_);
    renderer.drawText(panel.pos + Vec2{136.0f, 238.0f}, buffer, {255, 230, 150, 255}, 2);
    std::snprintf(buffer, sizeof(buffer), "クリア時間 %s   掘削数 %d   撃破数 %d",
        formatRunTime(runStats_.elapsedSeconds).c_str(),
        runStats_.dugTiles,
        runStats_.defeatedEnemies);
    renderer.drawText(panel.pos + Vec2{136.0f, 292.0f}, buffer, {198, 208, 202, 255}, 2);

    for (int i = 0; i < StageClearItemCount; ++i) {
        drawUiButton(renderer, stageClearItemRect(i), stageClearItemName(i), i == stageClearSelection_, uiActionButtonStyle());
    }
    if (!stageClearStatus_.empty()) {
        renderer.drawText(panel.pos + Vec2{152.0f, 474.0f}, stageClearStatus_, {255, 230, 150, 255}, 2);
    }
}

void Game::renderSpellRingForeground(
    Renderer& renderer,
    const std::vector<const SpellRingItem*>& runtimeItems,
    const std::vector<LightSource>&,
    float totalSeconds) const
{
    if (dungeonRingIntroActive()) {
        const float introProgress = dungeonRingIntroProgress();
        drawDungeonRingIntroOrbit(renderer, spellRing_, balance_, introProgress, totalSeconds);
        int itemIndex = 0;
        for (const SpellRingItem* itemPtr : runtimeItems) {
            if (itemPtr == nullptr) {
                continue;
            }
            drawDungeonRingIntroItem(
                renderer,
                spellRing_,
                objectCatalog_,
                *itemPtr,
                itemIndex,
                introProgress,
                totalSeconds);
            ++itemIndex;
        }
        return;
    }

    drawSpellRingOrbitLayer(renderer, spellRing_, balance_, totalSeconds, 0.78f);

    if (spellRing_.state() != SpellRingState::Normal) {
        renderer.drawLine(player_.position, spellRing_.center(), {150, 110, 80, 100});
    }

    for (const SpellRingItem* itemPtr : runtimeItems) {
        if (itemPtr == nullptr) {
            continue;
        }
        const SpellRingItem& item = *itemPtr;
        const int ringIndex = std::clamp(item.ringIndex, 0, SpellRingCount - 1);
        const RingShape ringShape = spellRing_.ringShapeForIndex(ringIndex);
        const int ringItemCount = static_cast<int>(spellRing_.itemsForRing(ringIndex).size());
        const float cometVisualScale = ringShape == RingShape::Comet
            ? std::clamp(1.0f - std::max(0, ringItemCount - 10) * 0.014f, 0.76f, 1.0f)
            : 1.0f;
        const Vec2 drawPosition = ringItemDrawPosition(item, totalSeconds);
        const ItemData* object = objectForRingItem(objectCatalog_, item);
        if (item.type == SpellRingItemType::Shovel) {
            if (renderer.hasIconSheet()) {
                const float iconSize = IconDrawSize * cometVisualScale;
                renderer.drawIcon(ShovelIconIndex, drawPosition - Vec2{iconSize * 0.5f, iconSize * 0.5f}, {iconSize, iconSize});
            } else {
                const bool drewImage = object != nullptr &&
                    drawObjectImage(
                        renderer,
                        *object,
                        drawPosition,
                        {RingObjectImageMaxSize * cometVisualScale, RingObjectImageMaxSize * cometVisualScale});
                if (!drewImage) {
                    renderer.fillCircle(drawPosition, item.hitRadius * cometVisualScale, {178, 184, 190, 255});
                    const Vec2 outward = normalize(item.worldPosition - spellRing_.center());
                    renderer.drawLine(drawPosition, drawPosition + outward * (15.0f * cometVisualScale), {90, 96, 102, 255});
                }
            }
        } else if (item.type == SpellRingItemType::Torch) {
            if (renderer.hasIconSheet()) {
                const float iconSize = IconDrawSize * cometVisualScale;
                renderer.drawIcon(TorchIconIndex, drawPosition - Vec2{iconSize * 0.5f, iconSize * 0.5f}, {iconSize, iconSize});
            } else {
                const bool drewImage = object != nullptr &&
                    drawObjectImage(
                        renderer,
                        *object,
                        drawPosition,
                        {RingObjectImageMaxSize * cometVisualScale, RingObjectImageMaxSize * cometVisualScale});
                if (!drewImage) {
                    renderer.fillCircle(drawPosition, item.hitRadius * cometVisualScale, {242, 122, 25, 255});
                    renderer.fillCircle(drawPosition + Vec2{2.0f, -2.0f} * cometVisualScale, 4.0f * cometVisualScale, {255, 238, 98, 255});
                }
            }
        } else {
            const bool drewImage = object != nullptr &&
                drawObjectImage(
                    renderer,
                    *object,
                    drawPosition,
                    {RingObjectImageMaxSize * cometVisualScale, RingObjectImageMaxSize * cometVisualScale});
            if (!drewImage) {
                renderer.fillCircle(drawPosition, item.hitRadius * cometVisualScale, {96, 122, 210, 255});
                renderer.drawCircle(drawPosition, item.hitRadius * cometVisualScale + 3.0f, {160, 202, 255, 255});
            }
        }
        if (item.hiddenDetectionRadius > 0.0f) {
            renderer.drawCircle(drawPosition, item.hiddenDetectionRadius, {126, 208, 255, 90});
        }
        if (item.treasureDetectionRadius > 0.0f) {
            renderer.drawCircle(drawPosition, item.treasureDetectionRadius, {255, 220, 92, 90});
        }
    }
}

void Game::render(Renderer& renderer, const Time& time)
{
    renderer.clear({5, 5, 8, 255});
    beginUiFrame(time.deltaSeconds());
    if (mode_ == ScreenMode::OpeningKamishibai) {
        renderOpeningKamishibai(renderer);
        finishUiFrame(renderer);
        renderDebugOverlay(renderer, time);
        renderScreenTransitionOverlay(renderer);
        renderer.present();
        return;
    }
    if (mode_ == ScreenMode::Title) {
        renderTitleScreen(renderer);
        finishUiFrame(renderer);
        renderDebugOverlay(renderer, time);
        renderScreenTransitionOverlay(renderer);
        renderer.present();
        return;
    }
    if (mode_ == ScreenMode::ObjectImageScaleEdit) {
        renderObjectImageScaleEditScreen(renderer);
        finishUiFrame(renderer);
        renderDebugOverlay(renderer, time);
        renderScreenTransitionOverlay(renderer);
        renderer.present();
        return;
    }
    if (basePresentationActive()) {
        renderBaseScreen(renderer);
        inventory_.render(renderer, player_, spellRing_, objectCatalog_, encyclopedia_);
        renderPauseMenu(renderer);
        renderRingScreen(renderer, time.totalSeconds());
        dialogue_.render(renderer, camera_.width(), camera_.height());
        finishUiFrame(renderer);
        renderBaseDebugOverlay(renderer, time);
        renderScreenTransitionOverlay(renderer);
        renderWorldLoadingScreen(renderer, time.totalSeconds());
        renderer.present();
        return;
    }

    renderer.setWorldSpace(&camera_);

    const std::vector<const SpellRingItem*> runtimeItems = spellRing_.runtimeItems();
    const bool ringIntroActive = dungeonRingIntroActive();
    const bool miningStartTransitionInDungeon =
        mode_ == ScreenMode::Playing &&
        screenTransition_.active() &&
        screenTransition_.target == ScreenTransitionTarget::MiningStart;
    const float ringIntroProgress = dungeonRingIntroProgress();
    std::vector<LightSource> itemLights;
    int runtimeItemIndex = 0;
    for (const SpellRingItem* itemPtr : runtimeItems) {
        if (itemPtr == nullptr) {
            continue;
        }
        const float introLightScale = ringIntroActive
            ? dungeonRingIntroItemReveal(ringIntroProgress, runtimeItemIndex, itemPtr->ringIndex)
            : (miningStartTransitionInDungeon ? 0.0f : 1.0f);
        if (introLightScale <= 0.001f) {
            ++runtimeItemIndex;
            continue;
        }
        const int ringIndex = std::clamp(itemPtr->ringIndex, 0, SpellRingCount - 1);
        const Vec2 lightPosition = ringIntroActive
            ? dungeonRingIntroItemGroundPosition(spellRing_, *itemPtr, ringIntroProgress, runtimeItemIndex, ringIndex)
            : itemPtr->worldPosition;
        const float phase = itemPtr->localAngle * 1.7f + static_cast<float>(itemPtr->ringIndex) * 2.3f;
        if (itemPtr->lightRadius > 0.0f) {
            itemLights.push_back({
                flickeredLightPosition(lightPosition, time.totalSeconds(), phase),
                flickeredLightRadius(itemPtr->lightRadius, time.totalSeconds(), phase) * introLightScale,
            });
        } else if (itemPtr->type == SpellRingItemType::Torch) {
            const float torchPhase = phase + 0.47f;
            itemLights.push_back({
                flickeredLightPosition(lightPosition, time.totalSeconds(), torchPhase),
                flickeredLightRadius(balance_.lightRadius, time.totalSeconds(), torchPhase) * introLightScale,
            });
        }
        ++runtimeItemIndex;
    }
    if (warpPointsEnabled_) {
        for (const WarpPoint& point : warpPoints_) {
            float radiusTiles = point.discovered ? point.discoveredLightRadiusTiles : point.undiscoveredLightRadiusTiles;
            if (point.lightRevealAnimating && point.lightRevealDuration > 0.0f) {
                const float t = std::clamp(point.lightRevealTimer / point.lightRevealDuration, 0.0f, 1.0f);
                const float inv = 1.0f - t;
                const float eased = 1.0f - inv * inv * inv;
                radiusTiles = point.undiscoveredLightRadiusTiles +
                    (point.discoveredLightRadiusTiles - point.undiscoveredLightRadiusTiles) * eased;
            }
            const float radiusPx = radiusTiles * static_cast<float>(balance::TileSize);
            const float phase = static_cast<float>(point.index) * 1.73f;
            itemLights.push_back({
                flickeredLightPosition(point.position, time.totalSeconds(), phase),
                flickeredLightRadius(radiusPx, time.totalSeconds(), phase),
            });
        }
        if (hasBossSpawnPoint_ && !bossSpawned_) {
            itemLights.push_back({
                flickeredLightPosition(bossSpawnPoint_, time.totalSeconds(), 4.8f),
                flickeredLightRadius(120.0f, time.totalSeconds(), 4.8f),
            });
        }
    }
    if (mode_ == ScreenMode::Playing && !enemyTestActive_) {
        const float entranceLightRadius = DungeonEntranceLightRadiusTiles * static_cast<float>(balance::TileSize);
        itemLights.push_back({
            flickeredLightPosition(dungeonEntrancePosition(), time.totalSeconds(), 2.9f),
            flickeredLightRadius(entranceLightRadius, time.totalSeconds(), 2.9f),
        });
    }
    tileMap_.render(renderer, camera_, player_.position, itemLights);
    std::vector<DepthRenderEntry> worldDepthEntries;
    if (!enemyTestActive_) {
        appendRewardNodeRenderEntries(worldDepthEntries, renderer, itemLights);
    }
    worldDrops_.appendRenderEntries(worldDepthEntries, renderer, tileMap_, objectCatalog_, player_.position, itemLights);
    effects_.appendRenderEntries(worldDepthEntries, renderer);
    if (!enemyTestActive_) {
        renderDungeonEntrance(renderer);
        renderWarpPoints(renderer);
    }

    const bool ringCenterVisible = tileMap_.isLit(spellRing_.center(), player_.position, itemLights);
    if (ringCenterVisible && !ringIntroActive) {
        drawSpellRingOrbitLayer(renderer, spellRing_, balance_, time.totalSeconds(), 0.46f);
    }
    if (spellRing_.state() != SpellRingState::Normal && ringCenterVisible) {
        renderer.drawLine(player_.position, spellRing_.center(), {150, 110, 80, 100});
    }

    const Vec2 playerFootAnchor = player_.position;
    renderer.drawActorShadow(playerFootAnchor, PlayerSpriteDrawSize);
    worldDrops_.renderShadows(renderer, tileMap_, objectCatalog_, player_.position, itemLights);
    enemies_.renderShadows(renderer, tileMap_, player_.position, itemLights);
    effects_.renderShadows(renderer);
    renderPlayerFootstepDust(renderer);
    worldDepthEntries.push_back(DepthRenderEntry{
        player_.position.y,
        [&]() {
            if (renderer.hasPlayerSheet()) {
                const int playerFrame = player_.spriteFrameIndex();
                const bool playerFlip = player_.facing.x > 0.0f;
                renderer.drawPlayerSprite(
                    playerFrame,
                    playerFootAnchor,
                    PlayerSpriteDrawSize,
                    playerFlip,
                    {255, 255, 255, 255},
                    {PlayerSpriteAnchorX, PlayerSpriteAnchorY});
                if (player_.damageFlash > 0.0f) {
                    const float flash = clamp(player_.damageFlash / 0.16f, 0.0f, 1.0f);
                    const unsigned char alpha = static_cast<unsigned char>(std::round(185.0f * flash));
                    renderer.drawPlayerSprite(
                        playerFrame,
                        playerFootAnchor,
                        PlayerSpriteDrawSize,
                        playerFlip,
                        {255, 52, 52, alpha},
                        {PlayerSpriteAnchorX, PlayerSpriteAnchorY});
                }
            } else {
                const Color playerColor = player_.damageFlash > 0.0f
                    ? Color{255, 72, 72, 255}
                    : Color{118, 72, 168, 255};
                renderer.fillCircle(player_.position, balance_.playerRadius, playerColor);
                renderer.drawLine(player_.position, player_.position + player_.facing * 22.0f, {235, 210, 255, 255});
            }

            if (ringIntroActive) {
                return;
            }
            for (const SpellRingItem* itemPtr : runtimeItems) {
                if (itemPtr == nullptr || !tileMap_.isLit(itemPtr->worldPosition, player_.position, itemLights)) {
                    continue;
                }
                const SpellRingItem& item = *itemPtr;
                const int ringIndex = std::clamp(item.ringIndex, 0, SpellRingCount - 1);
                const RingShape ringShape = spellRing_.ringShapeForIndex(ringIndex);
                const int ringItemCount = static_cast<int>(spellRing_.itemsForRing(ringIndex).size());
                const float cometVisualScale = ringShape == RingShape::Comet
                    ? std::clamp(1.0f - std::max(0, ringItemCount - 10) * 0.014f, 0.76f, 1.0f)
                    : 1.0f;
                const Vec2 drawPosition = ringItemDrawPosition(item, time.totalSeconds());
                renderer.drawActorShadow(
                    actorShadowAnchor(item.worldPosition, ItemShadowGroundOffsetY),
                    ringItemShadowVisualSize(item, time.totalSeconds()) * cometVisualScale);
                const ItemData* object = objectForRingItem(objectCatalog_, item);
                if (item.type == SpellRingItemType::Shovel) {
                    if (renderer.hasIconSheet()) {
                        const float iconSize = IconDrawSize * cometVisualScale;
                        renderer.drawIcon(ShovelIconIndex, drawPosition - Vec2{iconSize * 0.5f, iconSize * 0.5f}, {iconSize, iconSize});
                    } else {
                        const bool drewImage = object != nullptr &&
                            drawObjectImage(
                                renderer,
                                *object,
                                drawPosition,
                                {RingObjectImageMaxSize * cometVisualScale, RingObjectImageMaxSize * cometVisualScale});
                        if (!drewImage) {
                            renderer.fillCircle(drawPosition, item.hitRadius * cometVisualScale, {178, 184, 190, 255});
                            const Vec2 outward = normalize(item.worldPosition - spellRing_.center());
                            renderer.drawLine(drawPosition, drawPosition + outward * (15.0f * cometVisualScale), {90, 96, 102, 255});
                        }
                    }
                } else if (item.type == SpellRingItemType::Torch) {
                    if (renderer.hasIconSheet()) {
                        const float iconSize = IconDrawSize * cometVisualScale;
                        renderer.drawIcon(TorchIconIndex, drawPosition - Vec2{iconSize * 0.5f, iconSize * 0.5f}, {iconSize, iconSize});
                    } else {
                        const bool drewImage = object != nullptr &&
                            drawObjectImage(
                                renderer,
                                *object,
                                drawPosition,
                                {RingObjectImageMaxSize * cometVisualScale, RingObjectImageMaxSize * cometVisualScale});
                        if (!drewImage) {
                            renderer.fillCircle(drawPosition, item.hitRadius * cometVisualScale, {242, 122, 25, 255});
                            renderer.fillCircle(drawPosition + Vec2{2.0f, -2.0f} * cometVisualScale, 4.0f * cometVisualScale, {255, 238, 98, 255});
                        }
                    }
                } else {
                    const bool drewImage = object != nullptr &&
                        drawObjectImage(
                            renderer,
                            *object,
                            drawPosition,
                            {RingObjectImageMaxSize * cometVisualScale, RingObjectImageMaxSize * cometVisualScale});
                    if (!drewImage) {
                        renderer.fillCircle(drawPosition, item.hitRadius * cometVisualScale, {96, 122, 210, 255});
                        renderer.drawCircle(drawPosition, item.hitRadius * cometVisualScale + 3.0f, {160, 202, 255, 255});
                    }
                }
                if (item.hiddenDetectionRadius > 0.0f) {
                    renderer.drawCircle(drawPosition, item.hiddenDetectionRadius, {126, 208, 255, 90});
                }
                if (item.treasureDetectionRadius > 0.0f) {
                    renderer.drawCircle(drawPosition, item.treasureDetectionRadius, {255, 220, 92, 90});
                }
            }
        },
    });
    enemies_.appendRenderEntries(worldDepthEntries, renderer, tileMap_, player_.position, itemLights);
    std::stable_sort(worldDepthEntries.begin(), worldDepthEntries.end(), [](const DepthRenderEntry& left, const DepthRenderEntry& right) {
        return left.sortY < right.sortY;
    });
    for (const DepthRenderEntry& entry : worldDepthEntries) {
        entry.draw();
    }

    std::vector<DepthRenderEntry> projectileDepthEntries;
    projectiles_.appendRenderEntries(projectileDepthEntries, renderer, tileMap_, player_.position, itemLights);
    std::stable_sort(projectileDepthEntries.begin(), projectileDepthEntries.end(), [](const DepthRenderEntry& left, const DepthRenderEntry& right) {
        return left.sortY < right.sortY;
    });
    for (const DepthRenderEntry& entry : projectileDepthEntries) {
        entry.draw();
    }
    effects_.render(renderer);
    tileMap_.renderDarknessOverlay(renderer, camera_, player_.position, itemLights);
    renderSpellRingForeground(renderer, runtimeItems, itemLights, time.totalSeconds());
    effects_.renderForeground(renderer);
    effects_.renderDamagePopups(renderer);

    renderer.setScreenSpace();
    renderTopInfoBar(renderer);
    if (mode_ == ScreenMode::Playing) {
        renderRingStatusHud(renderer);
        renderDungeonLogs(renderer);
        renderDungeonStatusHud(renderer);
    }
    if (reloadNoticeTimer_ > 0.0f) {
        renderer.fillRect({18.0f, 170.0f}, {430.0f, 26.0f}, {0, 0, 0, 180});
        InlineItemTextStyle noticeStyle;
        noticeStyle.text = {255, 235, 150, 255};
        noticeStyle.scale = 2;
        noticeStyle.iconTextGap = 4.0f;
        noticeStyle.iconScale = 1.15f;
        drawInlineItemText(renderer, objectCatalog_, {26.0f, 176.0f}, reloadNotice_, noticeStyle);
    }
    if (debugPaused_) {
        renderer.fillRect({18.0f, 202.0f}, {190.0f, 28.0f}, {0, 0, 0, 190});
        renderer.drawText({28.0f, 208.0f}, "DEBUG PAUSED", {255, 230, 150, 255}, 2);
    }
    upgrades_.render(renderer, levels_);
    inventory_.render(renderer, player_, spellRing_, objectCatalog_, encyclopedia_);
    if (mode_ == ScreenMode::Playing) {
        inventory_.renderShortcutHud(renderer, spellRing_, camera_.width(), camera_.height());
        renderRingEquipFx(renderer);
    }
    renderWarpReturnUi(renderer);
    renderPauseMenu(renderer);
    renderRingScreen(renderer, time.totalSeconds());
    renderGameOverScreen(renderer);
    renderStageClearScreen(renderer);
    renderEnemyTestUi(renderer);
    if (mode_ == ScreenMode::Playing || mode_ == ScreenMode::Inventory || mode_ == ScreenMode::PauseMenu || mode_ == ScreenMode::Ring) {
        encyclopedia_.renderPopups(renderer, camera_);
    }
    dialogue_.render(renderer, camera_.width(), camera_.height());
    finishUiFrame(renderer);
    renderDebugOverlay(renderer, time);
    renderScreenTransitionOverlay(renderer);
    renderer.present();
}

} // namespace majo
