#pragma once

#include "engine/Input.hpp"
#include "engine/Math.hpp"
#include "engine/Renderer.hpp"

#include <string_view>

namespace majo {

namespace ui {
inline constexpr Color WindowFill{20, 36, 92, 206};
inline constexpr Color WindowFillStrong{14, 30, 82, 228};
inline constexpr Color HeaderFill{18, 10, 32, 238};
inline constexpr Color FooterFill{18, 10, 32, 218};
inline constexpr Color WindowBorder{255, 255, 255, 255};
inline constexpr Color Text{255, 255, 255, 255};
inline constexpr Color TextMuted{198, 198, 206, 255};
inline constexpr Color TextDisabled{150, 150, 160, 255};
inline constexpr Vec2 ButtonTextPadding{18.0f, 0.0f};
inline constexpr float ButtonHeight = 53.0f;
inline constexpr float HeaderHeight = 92.0f;
inline constexpr float FooterLineHeight = 24.0f;
inline constexpr float FooterPaddingY = 8.0f;
inline constexpr float FooterMaxHeight = FooterLineHeight * 2.0f + FooterPaddingY * 2.0f;
inline constexpr float PanelPadding = 24.0f;
inline constexpr float ButtonGap = 10.0f;
inline constexpr Vec2 HeaderTitlePadding{24.0f, 32.0f};
inline constexpr Vec2 FooterTextPadding{24.0f, 8.0f};
inline constexpr Vec2 ImageWindowHeaderTitlePadding{48.0f, 40.0f};
inline constexpr Vec2 ImageWindowFooterTextPadding{48.0f, 0.0f};
inline constexpr Vec2 SubPanelPadding{24.0f, 24.0f};
inline constexpr Vec2 CancelButtonSize{58.0f, 60.0f};
inline constexpr Vec2 CancelButtonOffset{-6.0f, 4.0f};
inline constexpr float SeparatorHeight = 36.0f;
inline constexpr float BodyMessageGap = 8.0f;
inline constexpr float WindowAnimationFrames = 20.0f;
inline constexpr float WindowAnimationSeconds = WindowAnimationFrames / 60.0f;
}

struct UiRect {
    Vec2 pos{};
    Vec2 size{};

    bool contains(Vec2 point) const;
};

class UiContext {
public:
    explicit UiContext(const Input& input);

    Vec2 mouse() const { return mouse_; }
    bool pointerConsumed() const { return pointerConsumed_; }
    bool hovered(UiRect rect) const;
    bool pressed(UiRect rect);
    void consumePointer() { pointerConsumed_ = true; }
    void block(UiRect rect);

private:
    Vec2 mouse_{};
    bool mouseLeftPressed_ = false;
    bool pointerConsumed_ = false;
};

struct UiButtonStyle {
    Color fill{30, 46, 104, 218};
    Color fillHot{40, 60, 126, 234};
    Color outline{255, 255, 255, 255};
    Color outlineHot{255, 255, 255, 255};
    Color text{255, 255, 255, 255};
    int imageVariant = 0;
};

struct UiGaugeGradient {
    Color start{108, 206, 236, 230};
    Color end{132, 230, 250, 230};
    GradientDirection direction = GradientDirection::LeftToRight;
};

struct UiGaugeStyle {
    UiGaugeGradient fill{};
    Color track{12, 16, 24, 190};
    Color trackInner{30, 38, 52, 220};
    Color trackOuter{218, 228, 244, 78};
    Color shadow{0, 0, 0, 105};
    Color tick{255, 255, 255, 0};
    Color highlight{255, 255, 255, 96};
    Color capGlow{132, 230, 250, 78};
    Color capCore{246, 252, 255, 225};
    Color shimmer{255, 255, 255, 0};
    float cornerRadius = -1.0f;
    float trackOuterExtra = 2.0f;
    float trackInnerInset = 5.0f;
    float shadowOffsetY = 3.0f;
    float shadowExtra = 8.0f;
    int tickCount = 0;
    float shimmerPhase = -1.0f;
    float shimmerWidth = 56.0f;
};

struct UiCommandMenuItem {
    std::string_view label;
    bool enabled = true;
};

struct UiCommandMenuState {
    bool open = false;
    bool visible = false;
    bool closing = false;
    UiRect panel{};
    int hoveredIndex = -1;
    int textScale = 2;
    float animation = 0.0f;
};

struct UiDropdownItem {
    std::string_view label;
    bool enabled = true;
};

struct UiDropdownState {
    bool open = false;
    int highlightedIndex = -1;
    int scrollOffset = 0;
};

struct UiDropdownStyle {
    int visibleRows = 8;
    float rowHeight = 38.0f;
    int textScale = 2;
    Color fill{18, 24, 40, 238};
    Color fillHot{36, 48, 74, 244};
    Color outline{255, 255, 255, 220};
    Color text{255, 255, 255, 255};
    Color textDisabled{150, 150, 160, 255};
    Color arrow{255, 255, 255, 230};
    Color scrollbarTrack{255, 255, 255, 48};
    Color scrollbarThumb{255, 255, 255, 170};
    std::string_view emptyLabel = "項目がありません";
};

struct UiWindowOptions {
    bool animated = true;
    bool cancelButton = false;
};

struct UiCancelControlState {
    bool backArmed = false;
    bool pointerArmed = false;
};

class UiCancelControlScope {
public:
    explicit UiCancelControlScope(UiCancelControlState& state);
    ~UiCancelControlScope();
    UiCancelControlScope(const UiCancelControlScope&) = delete;
    UiCancelControlScope& operator=(const UiCancelControlScope&) = delete;

private:
    UiCancelControlState* previous_ = nullptr;
};

inline UiButtonStyle uiActionButtonStyle()
{
    UiButtonStyle style;
    style.imageVariant = 1;
    return style;
}

inline UiButtonStyle uiCancelButtonStyle()
{
    UiButtonStyle style;
    style.imageVariant = 2;
    return style;
}

void beginUiFrame(float dt);
void finishUiFrame(Renderer& renderer);

class UiWindowScope {
public:
    UiWindowScope(
        Renderer& renderer,
        std::string_view id,
        UiRect panel,
        std::string_view title,
        std::string_view helpText = {},
        bool animated = true);
    UiWindowScope(
        Renderer& renderer,
        std::string_view id,
        UiRect panel,
        std::string_view title,
        std::string_view helpText,
        UiWindowOptions options);
    ~UiWindowScope();
    UiWindowScope(const UiWindowScope&) = delete;
    UiWindowScope& operator=(const UiWindowScope&) = delete;
    UiWindowScope(UiWindowScope&& other) noexcept;
    UiWindowScope& operator=(UiWindowScope&& other) noexcept;

private:
    Renderer* renderer_ = nullptr;
    bool transformed_ = false;
};

UiRect uiHeaderRect(UiRect panel);
float uiFooterHeight(std::string_view helpText);
UiRect uiFooterRect(UiRect panel, std::string_view helpText = {});
UiRect uiBodyRect(UiRect panel);
Vec2 uiSubPanelContentPos(UiRect panel);
UiRect uiSubPanelContentRect(UiRect panel);
UiRect uiBottomLeftButtonRect(UiRect panel, Vec2 size);
UiRect uiBottomCenterButtonRect(UiRect panel, Vec2 size);
UiRect uiBottomRightButtonRect(UiRect panel, Vec2 size);
UiRect uiCancelButtonRect(UiRect panel);
bool uiCancelRequested(UiCancelControlState& state, const Input& input, UiContext& ui, UiRect panel);

void drawUiPanel(Renderer& renderer, UiRect panel);
void drawUiSubPanel(Renderer& renderer, UiRect panel);
void drawUiHeader(Renderer& renderer, UiRect panel, std::string_view title);
void drawUiFooter(Renderer& renderer, UiRect panel, std::string_view helpText);
void drawUiWindow(Renderer& renderer, UiRect panel, std::string_view title, std::string_view helpText = {});
void drawUiCancelButton(Renderer& renderer, UiRect panel);
void drawUiSeparator(Renderer& renderer, UiRect rect, Color tint = {255, 255, 255, 255});
void drawUiGauge(Renderer& renderer, UiRect rect, float progress, const UiGaugeStyle& style = {});
void drawUiButton(Renderer& renderer, UiRect rect, std::string_view label, bool hot, const UiButtonStyle& style = {});
void drawUiRectButton(Renderer& renderer, UiRect rect, std::string_view label, bool hot, const UiButtonStyle& style = {});
void drawUiBodyMessageBelow(Renderer& renderer, UiRect anchor, std::string_view message, Color color = ui::TextMuted);
float drawUiDetailHeader(Renderer& renderer, UiRect panel, std::string_view text);
void drawUiDetailText(Renderer& renderer, UiRect panel, float& y, std::string_view text);
void drawUiDetailLine(Renderer& renderer, UiRect panel, float& y, std::string_view label, std::string_view value, Color valueColor = ui::Text);
void openUiCommandMenu(
    UiCommandMenuState& state,
    Vec2 anchor,
    UiRect bounds,
    int itemCount,
    const UiCommandMenuItem* items = nullptr,
    float minWidth = 120.0f,
    int textScale = 2);
void closeUiCommandMenu(UiCommandMenuState& state);
int updateUiCommandMenu(UiCommandMenuState& state, UiContext& ui, const Input& input, const UiCommandMenuItem* items, int itemCount);
void drawUiCommandMenu(Renderer& renderer, const UiCommandMenuState& state, const UiCommandMenuItem* items, int itemCount);
UiRect uiDropdownListRect(UiRect buttonRect, int itemCount, const UiDropdownStyle& style = {});
UiRect uiDropdownItemRect(UiRect buttonRect, int visibleIndex, const UiDropdownStyle& style = {});
int updateUiDropdown(
    UiDropdownState& state,
    UiContext& ui,
    const Input& input,
    UiRect buttonRect,
    int selectedIndex,
    const UiDropdownItem* items,
    int itemCount,
    const UiDropdownStyle& style = {});
void drawUiDropdown(
    Renderer& renderer,
    const UiDropdownState& state,
    UiRect buttonRect,
    std::string_view selectedLabel,
    const UiDropdownItem* items,
    int itemCount,
    const UiDropdownStyle& style = {});

}
