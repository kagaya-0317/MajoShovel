#pragma once

#include "data/ObjectCatalog.hpp"
#include "engine/Ui.hpp"
#include "game/ItemModel.hpp"
#include "game/ObjectImageRenderer.hpp"
#include "game/SpellRingItem.hpp"
#include "game/SpellRingSystem.hpp"

#include <optional>
#include <span>
#include <string>
#include <vector>

namespace majo {

class EncyclopediaSystem;

struct InventoryUiItemStats {
    std::string instanceId;
    int currentDurability = -1;
    int maxDurability = -1;
    int enhanceLevel = 0;
    int attackEnhanceLevel = 0;
    int digEnhanceLevel = 0;
    int durabilityEnhanceLevel = 0;
    int attackBonus = 0;
    int digBonus = 0;
    int durabilityBonus = 0;
    double weightModifier = 1.0;
    double sizeModifier = 1.0;
    bool protectionEnabled = false;
    bool broken = false;
};

struct InventoryUiEntryView {
    const ItemData* item = nullptr;
    const ItemInstance* instance = nullptr;
    std::optional<InventoryUiItemStats> stats;
    int stackCount = 0;
    bool equipped = false;
};

inline constexpr std::string_view InventoryUiProtectionStatusText = "保護中";

struct InventoryUiSlotStyle {
    bool selected = false;
    bool disabled = false;
    float imageMaxSize = 48.0f;
    float contentAlpha = 1.0f;
    std::string bottomLabel;
    Color bottomLabelColor = ui::Text;
    bool showTopRightCount = false;
    int topRightCount = 0;
    Color topRightCountColor = ui::Text;
    bool showProtectionLabel = true;
    Color protectionLabelColor = ui::Text;
    bool showFrame = true;
};

struct InventoryUiDetailExtraLine {
    std::string label;
    std::string value;
    Color valueColor = ui::Text;
};

struct InventoryUiDetailOptions {
    bool showEnhanceCount = true;
    bool showProtectionLabel = true;
    float animationSeconds = 0.0f;
    bool showExtraLineSeparator = true;
    int unlockedRingCount = SpellRingCount;
};

struct InventoryUiGridStyle {
    int columns = 8;
    int visibleRows = 3;
    Vec2 slotSize{88.0f, 76.0f};
    Vec2 slotGap{8.0f, 8.0f};
    float imageMaxSize = 48.0f;
    UiScrollAreaStyle scroll{};
};

struct InventoryUiScreenLayout {
    UiRect window{{44.0f, 58.0f}, {1192.0f, 610.0f}};
    UiRect backdrop{{0.0f, 0.0f}, {1280.0f, 720.0f}};
    Vec2 gridOrigin{};
    UiRect detailPanel{};
    InventoryUiGridStyle grid{};
    float itemImageMaxSize = 48.0f;
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
[[nodiscard]] std::string itemDisplayName(std::string_view baseName, bool broken);
[[nodiscard]] Color itemFallbackColorForBrokenState(Color color, bool broken);
[[nodiscard]] ObjectImageDrawOptions itemImageOptionsWithBrokenState(ObjectImageDrawOptions options, bool broken);
[[nodiscard]] InventoryUiItemStats inventoryUiStatsFromInstance(const ItemInstance& instance);
[[nodiscard]] InventoryUiItemStats inventoryUiStatsFromRingItem(const SpellRingItem& item);
[[nodiscard]] std::optional<InventoryUiItemStats> inventoryUiEntryStats(const InventoryUiEntryView& entry);
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

void drawInventoryUiSlotBottomLabel(
    Renderer& renderer,
    UiRect rect,
    std::string_view label,
    Color color = ui::Text);

void drawInventoryUiProtectionLabel(
    Renderer& renderer,
    UiRect rect,
    Color color = ui::Text,
    float alphaScale = 1.0f);

void drawInventoryUiItemIcon(
    Renderer& renderer,
    Vec2 center,
    const InventoryUiEntryView& entry,
    float imageMaxSize,
    bool selected = false,
    bool disabled = false,
    float alphaScale = 1.0f);

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

[[nodiscard]] int inventoryUiGridRowCount(int itemCount, const InventoryUiGridStyle& style = {});
[[nodiscard]] float inventoryUiGridWidth(const InventoryUiGridStyle& style = {});
[[nodiscard]] float inventoryUiGridVisibleHeight(const InventoryUiGridStyle& style = {});
[[nodiscard]] UiRect inventoryUiGridViewport(Vec2 pos, const InventoryUiGridStyle& style = {});
[[nodiscard]] float inventoryUiGridContentHeight(int itemCount, const InventoryUiGridStyle& style = {});
[[nodiscard]] UiScrollAreaLayout makeInventoryUiGridLayout(
    UiRect viewport,
    int itemCount,
    float scrollOffset,
    const InventoryUiGridStyle& style = {});
[[nodiscard]] UiScrollAreaLayout updateInventoryUiGrid(
    UiContext& ui,
    const Input& input,
    UiRect viewport,
    int itemCount,
    float& scrollOffset,
    const InventoryUiGridStyle& style = {},
    UiScrollAreaState* state = nullptr);
[[nodiscard]] UiRect inventoryUiGridSlotRect(
    const UiScrollAreaLayout& layout,
    int index,
    const InventoryUiGridStyle& style = {});
[[nodiscard]] const InventoryUiScreenLayout& standardInventoryUiScreenLayout();
[[nodiscard]] UiRect inventoryUiScreenSlotRect(const InventoryUiScreenLayout& layout, int index);
void keepInventoryUiGridItemVisible(
    UiRect viewport,
    int selectedIndex,
    int itemCount,
    float& scrollOffset,
    const InventoryUiGridStyle& style = {});
void drawInventoryUiGrid(
    Renderer& renderer,
    const UiScrollAreaLayout& layout,
    std::span<const InventoryUiEntryView> entries,
    int selectedIndex,
    const InventoryUiGridStyle& style = {});

void drawInventoryUiDetailPanel(
    Renderer& renderer,
    UiRect panel,
    const InventoryUiEntryView& entry,
    const ObjectCatalog& catalog,
    const EncyclopediaSystem& encyclopedia,
    const InventoryUiDetailOptions& options = {},
    const std::vector<InventoryUiDetailExtraLine>& extraLines = {});

}
