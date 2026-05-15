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
    bool showProtectionLabel = true;
    Color protectionLabelColor = ui::Text;
};

struct InventoryUiDetailExtraLine {
    std::string label;
    std::string value;
    Color valueColor = ui::Text;
};

struct InlineItemTextStyle {
    Color text{255, 255, 255, 255};
    int scale = 2;
    float iconTextGap = 4.0f;
    float iconScale = 1.0f;
    bool outlineEnabled = false;
    Color outline{0, 0, 0, 160};
    int outlinePx = 2;
};

[[nodiscard]] Color inventoryUiObjectColor(const ItemData& item);
[[nodiscard]] std::string joinInventoryUiEffectLines(const std::vector<std::string>& lines);
[[nodiscard]] Vec2 measureInlineItemText(
    Renderer& renderer,
    std::string_view text,
    const InlineItemTextStyle& style = {});
[[nodiscard]] std::string fittedInlineItemText(
    Renderer& renderer,
    std::string text,
    float maxWidth,
    const InlineItemTextStyle& style = {});
[[nodiscard]] std::string inlineItemTag(std::string_view objectId);
[[nodiscard]] std::string inlineWorldIconTag(std::string_view worldIconKey);
[[nodiscard]] std::string inlineMaterialIconTag(MaterialType type);

void drawInlineItemText(
    Renderer& renderer,
    const ObjectCatalog& catalog,
    Vec2 pos,
    std::string_view text,
    const InlineItemTextStyle& style = {});

void drawInlineItemTextRightAligned(
    Renderer& renderer,
    const ObjectCatalog& catalog,
    Vec2 rightTop,
    std::string_view text,
    const InlineItemTextStyle& style = {});

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
