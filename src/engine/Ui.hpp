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
inline constexpr Vec2 ButtonTextPadding{16.0f, 9.0f};
inline constexpr float HeaderHeight = 58.0f;
inline constexpr float FooterHeight = 36.0f;
inline constexpr float PanelPadding = 24.0f;
inline constexpr float ButtonGap = 10.0f;
inline constexpr Vec2 HeaderTitlePadding{24.0f, 14.0f};
inline constexpr Vec2 FooterTextPadding{24.0f, 8.0f};
inline constexpr float BodyMessageGap = 8.0f;
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
};

UiRect uiHeaderRect(UiRect panel);
UiRect uiFooterRect(UiRect panel);
UiRect uiBodyRect(UiRect panel);
UiRect uiBottomLeftButtonRect(UiRect panel, Vec2 size);
UiRect uiBottomRightButtonRect(UiRect panel, Vec2 size);

void drawUiPanel(Renderer& renderer, UiRect panel);
void drawUiHeader(Renderer& renderer, UiRect panel, std::string_view title);
void drawUiFooter(Renderer& renderer, UiRect panel, std::string_view helpText);
void drawUiWindow(Renderer& renderer, UiRect panel, std::string_view title, std::string_view helpText = {});
void drawUiButton(Renderer& renderer, UiRect rect, std::string_view label, bool hot, const UiButtonStyle& style = {});
void drawUiBodyMessageBelow(Renderer& renderer, UiRect anchor, std::string_view message, Color color = ui::TextMuted);

}
