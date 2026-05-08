#include "engine/Ui.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

namespace majo {

namespace {

struct UiWindowState {
    UiRect panel{};
    std::string title;
    std::string helpText;
    float openProgress = 0.0f;
    float closeProgress = 0.0f;
    bool seen = false;
    bool closing = false;
};

std::unordered_map<std::string, UiWindowState> windowStates;
float windowAnimationStep = 1.0f / ui::WindowAnimationFrames;

std::string windowKey(std::string_view id, UiRect panel)
{
    std::string key{id};
    key += ':';
    key += std::to_string(static_cast<int>(panel.pos.x));
    key += ',';
    key += std::to_string(static_cast<int>(panel.pos.y));
    key += ',';
    key += std::to_string(static_cast<int>(panel.size.x));
    key += ',';
    key += std::to_string(static_cast<int>(panel.size.y));
    return key;
}

Vec2 panelCenter(UiRect panel)
{
    return panel.pos + panel.size * 0.5f;
}

float easeOut(float t)
{
    t = clamp(t, 0.0f, 1.0f);
    return 1.0f - (1.0f - t) * (1.0f - t);
}

void applyWindowTransform(Renderer& renderer, UiRect panel, float scale, float alpha)
{
    renderer.pushScreenTransform(panelCenter(panel), scale, alpha);
}

void drawUiWindowChrome(Renderer& renderer, UiRect panel, std::string_view title, std::string_view helpText)
{
    drawUiPanel(renderer, panel);
    drawUiHeader(renderer, panel, title);
    drawUiFooter(renderer, panel, helpText);
}

Color scaledColor(Color color, float scale)
{
    color.r = static_cast<unsigned char>(std::clamp(std::lround(static_cast<float>(color.r) * scale), 0L, 255L));
    color.g = static_cast<unsigned char>(std::clamp(std::lround(static_cast<float>(color.g) * scale), 0L, 255L));
    color.b = static_cast<unsigned char>(std::clamp(std::lround(static_cast<float>(color.b) * scale), 0L, 255L));
    return color;
}

}

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

void beginUiFrame(float dt)
{
    const float duration = std::max(0.001f, ui::WindowAnimationSeconds);
    windowAnimationStep = clamp(dt / duration, 0.0f, 1.0f);
    for (auto& entry : windowStates) {
        entry.second.seen = false;
    }
}

void finishUiFrame(Renderer& renderer)
{
    renderer.setScreenSpace();
    std::vector<std::string> finished;
    for (auto& entry : windowStates) {
        UiWindowState& state = entry.second;
        if (state.seen) {
            continue;
        }
        state.closing = true;
        state.closeProgress = std::min(1.0f, state.closeProgress + windowAnimationStep);
        const float t = easeOut(state.closeProgress);
        const float scale = lerp(1.0f, 1.1f, t);
        const float alpha = 1.0f - t;
        applyWindowTransform(renderer, state.panel, scale, alpha);
        drawUiWindowChrome(renderer, state.panel, state.title, state.helpText);
        renderer.popScreenTransform();
        if (state.closeProgress >= 1.0f) {
            finished.push_back(entry.first);
        }
    }
    for (const std::string& key : finished) {
        windowStates.erase(key);
    }
}

UiWindowScope::UiWindowScope(
    Renderer& renderer,
    std::string_view id,
    UiRect panel,
    std::string_view title,
    std::string_view helpText,
    bool animated)
    : renderer_(&renderer)
{
    if (!animated) {
        drawUiWindowChrome(renderer, panel, title, helpText);
        return;
    }

    UiWindowState& state = windowStates[windowKey(id, panel)];
    if (state.closing) {
        state.openProgress = 0.0f;
        state.closeProgress = 0.0f;
        state.closing = false;
    }
    state.panel = panel;
    state.title = std::string(title);
    state.helpText = std::string(helpText);
    state.seen = true;
    state.openProgress = std::min(1.0f, state.openProgress + windowAnimationStep);

    const float t = easeOut(state.openProgress);
    applyWindowTransform(renderer, panel, lerp(0.9f, 1.0f, t), t);
    transformed_ = true;
    drawUiWindowChrome(renderer, panel, title, helpText);
}

UiWindowScope::~UiWindowScope()
{
    if (renderer_ != nullptr && transformed_) {
        renderer_->popScreenTransform();
    }
}

UiWindowScope::UiWindowScope(UiWindowScope&& other) noexcept
    : renderer_(other.renderer_)
    , transformed_(other.transformed_)
{
    other.renderer_ = nullptr;
    other.transformed_ = false;
}

UiWindowScope& UiWindowScope::operator=(UiWindowScope&& other) noexcept
{
    if (this == &other) {
        return *this;
    }
    if (renderer_ != nullptr && transformed_) {
        renderer_->popScreenTransform();
    }
    renderer_ = other.renderer_;
    transformed_ = other.transformed_;
    other.renderer_ = nullptr;
    other.transformed_ = false;
    return *this;
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

float uiFooterHeight(std::string_view helpText)
{
    if (helpText.empty()) {
        return 0.0f;
    }
    return (helpText.find('\n') == std::string_view::npos)
        ? ui::FooterLineHeight + ui::FooterPaddingY * 2.0f
        : ui::FooterMaxHeight;
}

UiRect uiFooterRect(UiRect panel, std::string_view helpText)
{
    const float height = uiFooterHeight(helpText);
    return {{panel.pos.x, panel.pos.y + panel.size.y - height}, {panel.size.x, height}};
}

UiRect uiBodyRect(UiRect panel)
{
    const float y = panel.pos.y + ui::HeaderHeight;
    const float footerHeight = ui::FooterMaxHeight;
    return {
        {panel.pos.x + ui::PanelPadding, y + ui::PanelPadding},
        {
            panel.size.x - ui::PanelPadding * 2.0f,
            panel.size.y - ui::HeaderHeight - footerHeight - ui::PanelPadding * 2.0f,
        },
    };
}

Vec2 uiSubPanelContentPos(UiRect panel)
{
    return panel.pos + ui::SubPanelPadding;
}

UiRect uiSubPanelContentRect(UiRect panel)
{
    return {
        uiSubPanelContentPos(panel),
        {
            std::max(0.0f, panel.size.x - ui::SubPanelPadding.x * 2.0f),
            std::max(0.0f, panel.size.y - ui::SubPanelPadding.y * 2.0f),
        },
    };
}

UiRect uiBottomLeftButtonRect(UiRect panel, Vec2 size)
{
    const UiRect body = uiBodyRect(panel);
    size.y = ui::ButtonHeight;
    return {{body.pos.x, body.pos.y + body.size.y - size.y}, size};
}

UiRect uiBottomCenterButtonRect(UiRect panel, Vec2 size)
{
    const UiRect body = uiBodyRect(panel);
    size.y = ui::ButtonHeight;
    return {{panel.pos.x + (panel.size.x - size.x) * 0.5f, body.pos.y + body.size.y - size.y}, size};
}

UiRect uiBottomRightButtonRect(UiRect panel, Vec2 size)
{
    const UiRect body = uiBodyRect(panel);
    size.y = ui::ButtonHeight;
    return {{body.pos.x + body.size.x - size.x, body.pos.y + body.size.y - size.y}, size};
}

void drawUiPanel(Renderer& renderer, UiRect panel)
{
    if (renderer.hasUiWindowTexture()) {
        renderer.drawUiWindowFrame(panel.pos, panel.size);
        return;
    }
    renderer.fillRect(panel.pos, panel.size, ui::WindowFill);
    renderer.drawRect(panel.pos, panel.size, ui::WindowBorder);
}

void drawUiSubPanel(Renderer& renderer, UiRect panel)
{
    if (renderer.hasUiSubWindowTexture()) {
        renderer.drawUiSubWindowFrame(panel.pos, panel.size);
        return;
    }
    renderer.fillRect(panel.pos, panel.size, ui::WindowFill);
    renderer.drawRect(panel.pos, panel.size, ui::WindowBorder);
}

void drawUiHeader(Renderer& renderer, UiRect panel, std::string_view title)
{
    const UiRect header = uiHeaderRect(panel);
    Vec2 titlePadding = ui::HeaderTitlePadding;
    if (!renderer.hasUiWindowTexture()) {
        renderer.fillRect(header.pos, header.size, ui::HeaderFill);
    } else {
        titlePadding = ui::ImageWindowHeaderTitlePadding;
    }
    renderer.drawText(header.pos + titlePadding, title, ui::Text, 3);
    renderer.drawText(header.pos + titlePadding + Vec2{1.0f, 0.0f}, title, ui::Text, 3);
}

void drawUiFooter(Renderer& renderer, UiRect panel, std::string_view helpText)
{
    if (helpText.empty()) {
        return;
    }
    const UiRect footer = uiFooterRect(panel, helpText);
    Vec2 textPadding = ui::FooterTextPadding;
    if (!renderer.hasUiWindowTexture()) {
        renderer.fillRect(footer.pos, footer.size, ui::FooterFill);
    } else {
        textPadding = ui::ImageWindowFooterTextPadding;
    }
    renderer.drawWrappedText(
        footer.pos + textPadding,
        helpText,
        footer.size.x - textPadding.x * 2.0f,
        ui::TextMuted,
        2);
}

void drawUiWindow(Renderer& renderer, UiRect panel, std::string_view title, std::string_view helpText)
{
    drawUiWindowChrome(renderer, panel, title, helpText);
}

void drawUiButton(Renderer& renderer, UiRect rect, std::string_view label, bool hot, const UiButtonStyle& style)
{
    rect.size.y = ui::ButtonHeight;
    const bool selected = hot;
    const float scale = selected ? 1.035f : 1.0f;
    const Vec2 center = rect.pos + rect.size * 0.5f;
    renderer.pushScreenTransform(center, scale, 1.0f);

    if (renderer.hasUiButtonTexture()) {
        Color tint = selected ? Color{255, 255, 235, 255} : Color{232, 232, 238, 255};
        renderer.drawUiButtonFrame(rect.pos, rect.size.x, style.imageVariant, tint);
    } else {
        Color fill = selected ? style.fillHot : style.fill;
        Color outline = selected ? scaledColor(style.outlineHot, 1.04f) : style.outline;
        renderer.fillRect(rect.pos, rect.size, fill);
        renderer.drawRect(rect.pos, rect.size, outline);
    }

    const Vec2 textSize = renderer.measureText(label, 2);
    const Vec2 textPos{
        rect.pos.x + std::max(0.0f, (rect.size.x - textSize.x) * 0.5f),
        rect.pos.y + std::max(0.0f, (rect.size.y - textSize.y) * 0.5f),
    };
    renderer.drawText(textPos, label, style.text, 2);
    renderer.popScreenTransform();
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

float drawUiDetailHeader(Renderer& renderer, UiRect panel, std::string_view text)
{
    constexpr float MinHeaderHeight = 50.0f;
    constexpr float HeaderGap = 16.0f;
    const UiRect content = uiSubPanelContentRect(panel);
    renderer.drawWrappedText(content.pos, text, content.size.x, ui::Text, 3);
    renderer.drawWrappedText({content.pos.x + 1.0f, content.pos.y}, text, content.size.x, ui::Text, 3);
    return content.pos.y + std::max(MinHeaderHeight, renderer.measureWrappedText(text, content.size.x, 3).y + HeaderGap);
}

void drawUiDetailText(Renderer& renderer, UiRect panel, float& y, std::string_view text)
{
    constexpr float TextGap = 8.0f;
    const UiRect content = uiSubPanelContentRect(panel);
    renderer.drawWrappedText({content.pos.x, y}, text, content.size.x, ui::Text, 2);
    y += renderer.measureWrappedText(text, content.size.x, 2).y + TextGap;
}

void drawUiDetailLine(Renderer& renderer, UiRect panel, float& y, std::string_view label, std::string_view value)
{
    constexpr float LabelWidth = 106.0f;
    constexpr float MinLineHeight = 31.0f;
    constexpr float LineGap = 4.0f;
    const float labelX = panel.pos.x + ui::SubPanelPadding.x;
    const float valueX = labelX + LabelWidth;
    const float valueMaxWidth = panel.pos.x + panel.size.x - valueX - ui::SubPanelPadding.x;
    renderer.drawText({labelX, y}, label, ui::TextMuted, 2);
    renderer.drawWrappedText({valueX, y}, value, valueMaxWidth, ui::Text, 2);
    y += std::max(MinLineHeight, renderer.measureWrappedText(value, valueMaxWidth, 2).y + LineGap);
}

}
