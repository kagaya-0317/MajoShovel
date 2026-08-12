#include "game/ItemGridInteraction.hpp"

#include <algorithm>

namespace majo {
namespace {

const ItemGridInteractionSlot* slotAt(
    std::span<const ItemGridInteractionSlot> slots,
    int index)
{
    if (index < 0 || index >= static_cast<int>(slots.size())) {
        return nullptr;
    }
    return &slots[static_cast<std::size_t>(index)];
}

int hoveredSlotIndex(
    std::span<const ItemGridInteractionSlot> slots,
    const UiContext& ui)
{
    for (int index = 0; index < static_cast<int>(slots.size()); ++index) {
        if (ui.pointerInside(slots[static_cast<std::size_t>(index)].rect)) {
            return index;
        }
    }
    return -1;
}

} // namespace

ItemGridInteractionResult ItemGridInteractionController::update(
    const ItemGridInteractionInput& interaction,
    const Input& input,
    UiContext& ui)
{
    const ItemGridInteractionSlot* selected = slotAt(interaction.slots, interaction.selectedSlot);
    int pressedSlot = -1;
    for (int index = 0; index < static_cast<int>(interaction.slots.size()); ++index) {
        if (ui.pressed(interaction.slots[static_cast<std::size_t>(index)].rect)) {
            pressedSlot = index;
        }
    }

    if (interaction.grabPressed) {
        cancelPointer();
        if (grabbedItem_) {
            return selected != nullptr
                ? requestMove(*selected, interaction.selectedSlot)
                : ItemGridInteractionResult{.event = ItemGridInteractionEvent::GrabCancelled, .consumed = true};
        }
        return selected != nullptr && selected->occupied()
            ? beginGrab(*selected, interaction.selectedSlot)
            : ItemGridInteractionResult{.event = ItemGridInteractionEvent::GrabCancelled, .consumed = true};
    }

    if (interaction.protectionPressed) {
        cancelPointer();
        if (grabbedItem_) {
            return {.event = ItemGridInteractionEvent::ProtectionBlocked, .item = *grabbedItem_, .consumed = true};
        }
        return selected != nullptr && selected->occupied()
            ? ItemGridInteractionResult{
                .event = ItemGridInteractionEvent::ProtectionRequested,
                .item = selected->key,
                .slotIndex = interaction.selectedSlot,
                .placement = selected->placement,
                .consumed = true}
            : ItemGridInteractionResult{.event = ItemGridInteractionEvent::ProtectionRequested, .consumed = true};
    }

    if (interaction.activatePressed) {
        cancelPointer();
        if (grabbedItem_) {
            return selected != nullptr
                ? requestMove(*selected, interaction.selectedSlot)
                : ItemGridInteractionResult{.event = ItemGridInteractionEvent::GrabCancelled, .consumed = true};
        }
        return selected != nullptr
            ? ItemGridInteractionResult{
                .event = ItemGridInteractionEvent::Activate,
                .item = selected->key,
                .slotIndex = interaction.selectedSlot,
                .placement = selected->placement,
                .consumed = true}
            : ItemGridInteractionResult{};
    }

    if (!interaction.pointerEnabled) {
        cancelPointer();
        return {};
    }

    const int hovered = hoveredSlotIndex(interaction.slots, ui);
    if (pressedSlot >= 0 && !ui.navigationActive()) {
        pointerPressSlot_ = pressedSlot;
        pointerPressPosition_ = input.mouseScreen();
        pointerCanActivate_ = true;
        pointerDragging_ = false;
        ui.consumePointer();
        return {.consumed = true};
    }

    const ItemGridInteractionSlot* pressed = slotAt(interaction.slots, pointerPressSlot_);
    if (pressed != nullptr &&
        input.mouseLeftHeld() &&
        !pointerDragging_ &&
        !grabbedItem_ &&
        pressed->occupied() &&
        lengthSquared(input.mouseScreen() - pointerPressPosition_) >=
            interaction.dragStartDistanceSquared) {
        pointerDragging_ = true;
        pointerCanActivate_ = false;
        return beginGrab(*pressed, pointerPressSlot_);
    }

    if (pointerPressSlot_ >= 0 && input.mouseLeftReleased()) {
        ItemGridInteractionResult result{};
        if (pointerDragging_ && grabbedItem_) {
            const ItemGridInteractionSlot* destination = slotAt(interaction.slots, hovered);
            result = destination != nullptr
                ? requestMove(*destination, hovered)
                : ItemGridInteractionResult{
                    .event = ItemGridInteractionEvent::GrabCancelled,
                    .item = *grabbedItem_,
                    .consumed = true};
        } else if (pointerCanActivate_ && hovered == pointerPressSlot_ && pressed != nullptr) {
            result = {
                .event = ItemGridInteractionEvent::Activate,
                .item = pressed->key,
                .slotIndex = pointerPressSlot_,
                .placement = pressed->placement,
                .consumed = true,
            };
        }
        cancelPointer();
        if (result.event == ItemGridInteractionEvent::GrabCancelled) {
            cancelGrab();
        }
        return result;
    }

    return {.consumed = pointerPressSlot_ >= 0};
}

bool ItemGridInteractionController::grabActive() const
{
    return grabbedItem_.has_value();
}

const ItemKey* ItemGridInteractionController::grabbedItem() const
{
    return grabbedItem_ ? &*grabbedItem_ : nullptr;
}

bool ItemGridInteractionController::isGrabbed(const ItemKey& key) const
{
    return grabbedItem_ && key.valid() && *grabbedItem_ == key;
}

int ItemGridInteractionController::grabbedOriginPlacement() const
{
    return grabbedOriginPlacement_;
}

bool ItemGridInteractionController::cancelGrab()
{
    if (!grabbedItem_) {
        return false;
    }
    grabbedItem_.reset();
    grabbedOriginPlacement_ = -1;
    return true;
}

void ItemGridInteractionController::cancelPointer()
{
    pointerPressSlot_ = -1;
    pointerPressPosition_ = {};
    pointerCanActivate_ = false;
    pointerDragging_ = false;
}

void ItemGridInteractionController::clear()
{
    (void)cancelGrab();
    cancelPointer();
}

ItemGridInteractionResult ItemGridInteractionController::beginGrab(
    const ItemGridInteractionSlot& slot,
    int slotIndex)
{
    grabbedItem_ = slot.key;
    grabbedOriginPlacement_ = slot.placement;
    return {
        .event = ItemGridInteractionEvent::GrabStarted,
        .item = slot.key,
        .slotIndex = slotIndex,
        .placement = slot.placement,
        .consumed = true,
    };
}

ItemGridInteractionResult ItemGridInteractionController::requestMove(
    const ItemGridInteractionSlot& destination,
    int slotIndex)
{
    if (!grabbedItem_) {
        return {};
    }
    const ItemKey item = *grabbedItem_;
    const int originPlacement = grabbedOriginPlacement_;
    cancelGrab();
    return {
        .event = ItemGridInteractionEvent::MoveRequested,
        .item = item,
        .slotIndex = slotIndex,
        .placement = destination.placement,
        .originPlacement = originPlacement,
        .destinationOccupied = destination.occupied(),
        .consumed = true,
    };
}

float itemGridInteractionContentAlpha(
    const ItemGridInteractionController& interaction,
    const ItemKey& key,
    float grabbedOriginAlpha)
{
    return interaction.isGrabbed(key)
        ? std::clamp(grabbedOriginAlpha, 0.0f, 1.0f)
        : 1.0f;
}

} // namespace majo
