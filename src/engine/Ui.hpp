#pragma once

#include "engine/Input.hpp"
#include "engine/Math.hpp"
#include "engine/Renderer.hpp"

#include <string_view>

namespace majo {

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
    Color fill{24, 24, 34, 230};
    Color fillHot{42, 34, 62, 245};
    Color outline{90, 84, 110, 255};
    Color outlineHot{255, 230, 150, 255};
    Color text{235, 235, 240, 255};
};

void drawUiButton(Renderer& renderer, UiRect rect, std::string_view label, bool hot, const UiButtonStyle& style = {});

}
