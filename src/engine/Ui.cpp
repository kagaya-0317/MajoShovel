#include "engine/Ui.hpp"

namespace majo {

bool UiRect::contains(Vec2 point) const
{
    return point.x >= pos.x &&
        point.y >= pos.y &&
        point.x < pos.x + size.x &&
        point.y < pos.y + size.y;
}

UiContext::UiContext(const Input& input)
    : mouse_(input.mouseScreen())
    , mouseLeftPressed_(input.mouseLeftPressed())
{
}

bool UiContext::hovered(UiRect rect) const
{
    return !pointerConsumed_ && rect.contains(mouse_);
}

bool UiContext::pressed(UiRect rect)
{
    const bool hit = hovered(rect);
    if (hit && mouseLeftPressed_) {
        pointerConsumed_ = true;
        return true;
    }
    return false;
}

void UiContext::block(UiRect rect)
{
    if (mouseLeftPressed_ && rect.contains(mouse_)) {
        pointerConsumed_ = true;
    }
}

void drawUiButton(Renderer& renderer, UiRect rect, std::string_view label, bool hot, const UiButtonStyle& style)
{
    renderer.fillRect(rect.pos, rect.size, hot ? style.fillHot : style.fill);
    renderer.drawRect(rect.pos, rect.size, hot ? style.outlineHot : style.outline);
    renderer.drawText(rect.pos + Vec2{12.0f, 10.0f}, label, style.text, 2);
}

}
