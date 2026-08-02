#pragma once

#include "engine/Input.hpp"
#include "engine/Ui.hpp"
#include "game/ItemCollectionTypes.hpp"

#include <optional>
#include <span>

namespace majo {

struct ItemGridInteractionSlot {
    UiRect rect{};
    ItemKey key{};
    int placement = -1;

    [[nodiscard]] bool occupied() const
    {
        return key.valid();
    }
};

enum class ItemGridInteractionEvent {
    None,
    Activate,
    GrabStarted,
    MoveRequested,
    ProtectionRequested,
    ProtectionBlocked,
    GrabCancelled,
};

struct ItemGridInteractionResult {
    ItemGridInteractionEvent event = ItemGridInteractionEvent::None;
    ItemKey item{};
    int slotIndex = -1;
    int placement = -1;
    int originPlacement = -1;
    bool destinationOccupied = false;
    bool consumed = false;
};

struct ItemGridInteractionInput {
    std::span<const ItemGridInteractionSlot> slots;
    int selectedSlot = -1;
    bool activatePressed = false;
    bool grabPressed = false;
    bool protectionPressed = false;
    bool pointerEnabled = true;
    float dragStartDistanceSquared = 36.0f;
};

class ItemGridInteractionController {
public:
    [[nodiscard]] ItemGridInteractionResult update(
        const ItemGridInteractionInput& interaction,
        const Input& input,
        UiContext& ui);

    [[nodiscard]] bool grabActive() const;
    [[nodiscard]] const ItemKey* grabbedItem() const;
    [[nodiscard]] bool isGrabbed(const ItemKey& key) const;
    [[nodiscard]] int grabbedOriginPlacement() const;

    bool cancelGrab();
    void cancelPointer();
    void clear();

private:
    [[nodiscard]] ItemGridInteractionResult beginGrab(
        const ItemGridInteractionSlot& slot,
        int slotIndex);
    [[nodiscard]] ItemGridInteractionResult requestMove(
        const ItemGridInteractionSlot& destination,
        int slotIndex);

    std::optional<ItemKey> grabbedItem_;
    int grabbedOriginPlacement_ = -1;
    int pointerPressSlot_ = -1;
    Vec2 pointerPressPosition_{};
    bool pointerCanActivate_ = false;
    bool pointerDragging_ = false;
};

[[nodiscard]] float itemGridInteractionContentAlpha(
    const ItemGridInteractionController& interaction,
    const ItemKey& key,
    float grabbedOriginAlpha = 0.42f);

} // namespace majo
