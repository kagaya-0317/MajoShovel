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

void drawUiPanel(Renderer& renderer, UiRect panel);
void drawUiSubPanel(Renderer& renderer, UiRect panel);
void drawUiHeader(Renderer& renderer, UiRect panel, std::string_view title);
void drawUiFooter(Renderer& renderer, UiRect panel, std::string_view helpText);
void drawUiWindow(Renderer& renderer, UiRect panel, std::string_view title, std::string_view helpText = {});
void drawUiButton(Renderer& renderer, UiRect rect, std::string_view label, bool hot, const UiButtonStyle& style = {});
void drawUiBodyMessageBelow(Renderer& renderer, UiRect anchor, std::string_view message, Color color = ui::TextMuted);
float drawUiDetailHeader(Renderer& renderer, UiRect panel, std::string_view text);
void drawUiDetailText(Renderer& renderer, UiRect panel, float& y, std::string_view text);
void drawUiDetailLine(Renderer& renderer, UiRect panel, float& y, std::string_view label, std::string_view value);

}
