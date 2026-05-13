#pragma once

#include "data/ObjectCatalog.hpp"
#include "engine/Ui.hpp"
#include "game/ItemModel.hpp"

#include <string>
#include <vector>

namespace majo {

class EncyclopediaSystem;

struct InventoryUiEntryView {
    const ItemData* item = nullptr;
    const ItemInstance* instance = nullptr;
    int stackCount = 0;
};

struct InventoryUiSlotStyle {
    bool selected = false;
    bool disabled = false;
    float imageMaxSize = 48.0f;
    std::string bottomLabel;
    Color bottomLabelColor = ui::Text;
    bool showTopRightCount = false;
    int topRightCount = 0;
    Color topRightCountColor = ui::Text;
};

struct InventoryUiDetailExtraLine {
    std::string label;
    std::string value;
    Color valueColor = ui::Text;
};

[[nodiscard]] Color inventoryUiObjectColor(const ItemData& item);
[[nodiscard]] std::string joinInventoryUiEffectLines(const std::vector<std::string>& lines);

void drawInventoryUiSlot(
    Renderer& renderer,
    UiRect rect,
    const InventoryUiEntryView& entry,
    const InventoryUiSlotStyle& style);

void drawInventoryUiSlot(
    Renderer& renderer,
    UiRect rect,
    const InventoryUiEntryView& entry,
    bool selected,
    float imageMaxSize);

void drawInventoryUiDetailPanel(
    Renderer& renderer,
    UiRect panel,
    const InventoryUiEntryView& entry,
    const ObjectCatalog& catalog,
    const EncyclopediaSystem& encyclopedia,
    bool showProtectionOperation,
    const std::vector<InventoryUiDetailExtraLine>& extraLines = {});

}
