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

UiRect uiHeaderRect(UiRect panel)
{
    return {panel.pos, {panel.size.x, ui::HeaderHeight}};
}

UiRect uiFooterRect(UiRect panel)
{
    return {{panel.pos.x, panel.pos.y + panel.size.y - ui::FooterHeight}, {panel.size.x, ui::FooterHeight}};
}

UiRect uiBodyRect(UiRect panel)
{
    const float y = panel.pos.y + ui::HeaderHeight;
    return {
        {panel.pos.x + ui::PanelPadding, y + ui::PanelPadding},
        {
            panel.size.x - ui::PanelPadding * 2.0f,
            panel.size.y - ui::HeaderHeight - ui::FooterHeight - ui::PanelPadding * 2.0f,
        },
    };
}

UiRect uiBottomLeftButtonRect(UiRect panel, Vec2 size)
{
    const UiRect body = uiBodyRect(panel);
    return {{body.pos.x, body.pos.y + body.size.y - size.y}, size};
}

UiRect uiBottomRightButtonRect(UiRect panel, Vec2 size)
{
    const UiRect body = uiBodyRect(panel);
    return {{body.pos.x + body.size.x - size.x, body.pos.y + body.size.y - size.y}, size};
}

void drawUiPanel(Renderer& renderer, UiRect panel)
{
    renderer.fillRect(panel.pos, panel.size, ui::WindowFill);
    renderer.drawRect(panel.pos, panel.size, ui::WindowBorder);
}

void drawUiHeader(Renderer& renderer, UiRect panel, std::string_view title)
{
    const UiRect header = uiHeaderRect(panel);
    renderer.fillRect(header.pos, header.size, ui::HeaderFill);
    renderer.drawText(header.pos + ui::HeaderTitlePadding, title, ui::Text, 3);
    renderer.drawText(header.pos + ui::HeaderTitlePadding + Vec2{1.0f, 0.0f}, title, ui::Text, 3);
}

void drawUiFooter(Renderer& renderer, UiRect panel, std::string_view helpText)
{
    if (helpText.empty()) {
        return;
    }
    const UiRect footer = uiFooterRect(panel);
    renderer.fillRect(footer.pos, footer.size, ui::FooterFill);
    renderer.drawText(footer.pos + ui::FooterTextPadding, helpText, ui::TextMuted, 2);
}

void drawUiWindow(Renderer& renderer, UiRect panel, std::string_view title, std::string_view helpText)
{
    drawUiPanel(renderer, panel);
    drawUiHeader(renderer, panel, title);
    drawUiFooter(renderer, panel, helpText);
}

void drawUiButton(Renderer& renderer, UiRect rect, std::string_view label, bool hot, const UiButtonStyle& style)
{
    renderer.fillRect(rect.pos, rect.size, hot ? style.fillHot : style.fill);
    renderer.drawRect(rect.pos, rect.size, hot ? style.outlineHot : style.outline);
    renderer.drawText(rect.pos + ui::ButtonTextPadding, label, style.text, 2);
}

void drawUiBodyMessageBelow(Renderer& renderer, UiRect anchor, std::string_view message, Color color)
{
    if (message.empty()) {
        return;
    }
    const Vec2 size = renderer.measureText(message, 2);
    const Vec2 pos{
        anchor.pos.x + (anchor.size.x - size.x) * 0.5f,
        anchor.pos.y + anchor.size.y + ui::BodyMessageGap,
    };
    renderer.drawText(pos, message, color, 2);
}

}
