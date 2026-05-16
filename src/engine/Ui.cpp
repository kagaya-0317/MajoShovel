#include "engine/Ui.hpp"

#include <SDL3/SDL.h>
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
    bool cancelButton = false;
};

UiCancelControlState* activeCancelState = nullptr;

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

void drawUiWindowChrome(Renderer& renderer, UiRect panel, std::string_view title, std::string_view helpText, bool cancelButton)
{
    drawUiPanel(renderer, panel);
    drawUiHeader(renderer, panel, title);
    drawUiFooter(renderer, panel, helpText);
    if (cancelButton) {
        drawUiCancelButton(renderer, panel);
    }
}

Color scaledColor(Color color, float scale)
{
    color.r = static_cast<unsigned char>(std::clamp(std::lround(static_cast<float>(color.r) * scale), 0L, 255L));
    color.g = static_cast<unsigned char>(std::clamp(std::lround(static_cast<float>(color.g) * scale), 0L, 255L));
    color.b = static_cast<unsigned char>(std::clamp(std::lround(static_cast<float>(color.b) * scale), 0L, 255L));
    return color;
}

bool colorVisible(Color color)
{
    return color.a != 0;
}

Color alphaScaledColor(Color color, float scale)
{
    color.a = static_cast<unsigned char>(std::clamp(std::lround(static_cast<float>(color.a) * scale), 0L, 255L));
    return color;
}

Color lerpColor(Color a, Color b, float t)
{
    t = clamp(t, 0.0f, 1.0f);
    return {
        static_cast<unsigned char>(std::clamp(std::lround(static_cast<float>(a.r) + (static_cast<float>(b.r) - static_cast<float>(a.r)) * t), 0L, 255L)),
        static_cast<unsigned char>(std::clamp(std::lround(static_cast<float>(a.g) + (static_cast<float>(b.g) - static_cast<float>(a.g)) * t), 0L, 255L)),
        static_cast<unsigned char>(std::clamp(std::lround(static_cast<float>(a.b) + (static_cast<float>(b.b) - static_cast<float>(a.b)) * t), 0L, 255L)),
        static_cast<unsigned char>(std::clamp(std::lround(static_cast<float>(a.a) + (static_cast<float>(b.a) - static_cast<float>(a.a)) * t), 0L, 255L)),
    };
}

void drawCapsuleOutline(Renderer& renderer, UiRect rect, float radius, Color color)
{
    if (!colorVisible(color) || rect.size.x <= 0.0f || rect.size.y <= 0.0f) {
        return;
    }

    const float centerY = rect.pos.y + rect.size.y * 0.5f;
    const float leftX = rect.pos.x + radius;
    const float rightX = rect.pos.x + rect.size.x - radius;
    if (rightX <= leftX) {
        renderer.drawCircle(rect.pos + rect.size * 0.5f, radius, color);
        return;
    }

    renderer.drawLine({leftX, rect.pos.y}, {rightX, rect.pos.y}, color);
    renderer.drawLine({leftX, rect.pos.y + rect.size.y}, {rightX, rect.pos.y + rect.size.y}, color);

    constexpr int ArcSegments = 8;
    const Vec2 leftCenter{leftX, centerY};
    const Vec2 rightCenter{rightX, centerY};
    const auto pointOnCircle = [radius](Vec2 center, float angle) {
        return center + Vec2{std::cos(angle) * radius, std::sin(angle) * radius};
    };
    for (int i = 0; i < ArcSegments; ++i) {
        const float t0 = static_cast<float>(i) / static_cast<float>(ArcSegments);
        const float t1 = static_cast<float>(i + 1) / static_cast<float>(ArcSegments);
        const float leftA0 = Pi * 0.5f + Pi * t0;
        const float leftA1 = Pi * 0.5f + Pi * t1;
        const float rightA0 = -Pi * 0.5f + Pi * t0;
        const float rightA1 = -Pi * 0.5f + Pi * t1;
        renderer.drawLine(pointOnCircle(leftCenter, leftA0), pointOnCircle(leftCenter, leftA1), color);
        renderer.drawLine(pointOnCircle(rightCenter, rightA0), pointOnCircle(rightCenter, rightA1), color);
    }
}

constexpr float CommandMenuItemHeight = 36.0f;
constexpr float CommandMenuPaddingX = 28.0f;
constexpr float CommandMenuPaddingY = 24.0f;
constexpr float CommandMenuItemGap = 10.0f;
constexpr float CommandMenuExtraWidth = 40.0f;
constexpr float CommandMenuOpenSpeed = 1.7f;
constexpr float CommandMenuCloseSpeed = 1.15f;
constexpr float DropdownGap = 6.0f;
constexpr float DropdownPadding = 4.0f;
constexpr float DropdownTextPaddingX = 12.0f;
constexpr float DropdownArrowWidth = 22.0f;
constexpr float DropdownScrollbarWidth = 8.0f;
constexpr float DropdownScrollbarGap = 6.0f;
constexpr float DropdownScrollbarPaddingY = 5.0f;
constexpr float DropdownScrollbarMinThumbHeight = 22.0f;

int utf8CodepointCount(std::string_view text)
{
    int count = 0;
    for (unsigned char c : text) {
        if ((c & 0xC0u) != 0x80u) {
            ++count;
        }
    }
    return count;
}

UiRect commandMenuItemRect(const UiCommandMenuState& state, int index)
{
    const float x = state.panel.pos.x + CommandMenuPaddingX;
    const float y = state.panel.pos.y + CommandMenuPaddingY + static_cast<float>(index) * (CommandMenuItemHeight + CommandMenuItemGap);
    const float w = std::max(0.0f, state.panel.size.x - CommandMenuPaddingX * 2.0f);
    return {{x, y}, {w, CommandMenuItemHeight}};
}

int dropdownVisibleCount(int itemCount, const UiDropdownStyle& style)
{
    const int maxRows = std::max(1, style.visibleRows);
    return std::min(maxRows, std::max(1, itemCount));
}

int dropdownMaxScrollOffset(int itemCount, const UiDropdownStyle& style)
{
    return std::max(0, itemCount - dropdownVisibleCount(itemCount, style));
}

bool dropdownNeedsScrollbar(int itemCount, const UiDropdownStyle& style)
{
    return itemCount > dropdownVisibleCount(itemCount, style);
}

float dropdownScrollbarReserve(int itemCount, const UiDropdownStyle& style)
{
    return dropdownNeedsScrollbar(itemCount, style) ? DropdownScrollbarWidth + DropdownScrollbarGap : 0.0f;
}

void clampDropdownState(UiDropdownState& state, int itemCount, int selectedIndex, const UiDropdownStyle& style)
{
    if (itemCount <= 0) {
        state.highlightedIndex = -1;
        state.scrollOffset = 0;
        return;
    }

    if (state.highlightedIndex < 0 || state.highlightedIndex >= itemCount) {
        state.highlightedIndex = std::clamp(selectedIndex, 0, itemCount - 1);
    }
    state.scrollOffset = std::clamp(state.scrollOffset, 0, dropdownMaxScrollOffset(itemCount, style));
}

void keepDropdownHighlightVisible(UiDropdownState& state, int itemCount, const UiDropdownStyle& style)
{
    if (itemCount <= 0 || state.highlightedIndex < 0) {
        state.scrollOffset = 0;
        return;
    }
    const int visibleCount = dropdownVisibleCount(itemCount, style);
    if (state.highlightedIndex < state.scrollOffset) {
        state.scrollOffset = state.highlightedIndex;
    } else if (state.highlightedIndex >= state.scrollOffset + visibleCount) {
        state.scrollOffset = state.highlightedIndex - visibleCount + 1;
    }
    state.scrollOffset = std::clamp(state.scrollOffset, 0, dropdownMaxScrollOffset(itemCount, style));
}

void moveDropdownHighlight(UiDropdownState& state, int delta, int itemCount, const UiDropdownStyle& style)
{
    if (itemCount <= 0 || delta == 0) {
        return;
    }
    if (state.highlightedIndex < 0 || state.highlightedIndex >= itemCount) {
        state.highlightedIndex = delta > 0 ? 0 : itemCount - 1;
    } else {
        state.highlightedIndex = std::clamp(state.highlightedIndex + delta, 0, itemCount - 1);
    }
    keepDropdownHighlightVisible(state, itemCount, style);
}

void scrollDropdown(UiDropdownState& state, int delta, int itemCount, const UiDropdownStyle& style)
{
    if (itemCount <= 0 || delta == 0) {
        return;
    }

    state.scrollOffset = std::clamp(state.scrollOffset + delta, 0, dropdownMaxScrollOffset(itemCount, style));
    const int visibleCount = dropdownVisibleCount(itemCount, style);
    if (state.highlightedIndex >= 0) {
        state.highlightedIndex = std::clamp(
            state.highlightedIndex,
            state.scrollOffset,
            std::min(itemCount - 1, state.scrollOffset + visibleCount - 1));
    }
}

bool tabItemEnabled(const UiTabItem* items, int index)
{
    return items != nullptr && index >= 0 && items[index].enabled;
}

int firstEnabledTab(const UiTabItem* items, int itemCount)
{
    if (items == nullptr) {
        return -1;
    }
    for (int i = 0; i < itemCount; ++i) {
        if (items[i].enabled) {
            return i;
        }
    }
    return -1;
}

void clampTabFocus(UiTabsState& state, int selectedIndex, const UiTabItem* items, int itemCount)
{
    if (itemCount <= 0 || items == nullptr) {
        state.focusedIndex = -1;
        return;
    }
    if (!tabItemEnabled(items, state.focusedIndex)) {
        state.focusedIndex = tabItemEnabled(items, selectedIndex) ? selectedIndex : firstEnabledTab(items, itemCount);
    }
}

int nextEnabledTab(int start, int direction, const UiTabItem* items, int itemCount, bool wrap)
{
    if (items == nullptr || itemCount <= 0 || direction == 0) {
        return start;
    }

    int current = start;
    if (current < 0 || current >= itemCount) {
        current = direction > 0 ? -1 : itemCount;
    }
    for (int step = 0; step < itemCount; ++step) {
        int candidate = current + direction;
        if (candidate < 0 || candidate >= itemCount) {
            if (!wrap) {
                return start;
            }
            candidate = direction > 0 ? 0 : itemCount - 1;
        }
        if (items[candidate].enabled) {
            return candidate;
        }
        current = candidate;
    }
    return start;
}

void moveTabFocus(UiTabsState& state, int delta, const UiTabItem* items, int itemCount, const UiTabsStyle& style)
{
    const int direction = delta > 0 ? 1 : -1;
    const int steps = delta > 0 ? delta : -delta;
    for (int step = 0; step < steps; ++step) {
        state.focusedIndex = nextEnabledTab(state.focusedIndex, direction, items, itemCount, style.wrapKeyboard);
    }
}

void removeUtf8LastCodepoint(std::string& text)
{
    if (text.empty()) {
        return;
    }
    text.pop_back();
    while (!text.empty() && (static_cast<unsigned char>(text.back()) & 0xC0u) == 0x80u) {
        text.pop_back();
    }
}

std::string fittedUiText(Renderer& renderer, std::string_view text, float maxWidth, int scale)
{
    if (text.empty() || renderer.measureText(text, scale).x <= maxWidth) {
        return std::string(text);
    }

    std::string result{text};
    constexpr std::string_view Ellipsis = "...";
    while (!result.empty() && renderer.measureText(result + std::string(Ellipsis), scale).x > maxWidth) {
        removeUtf8LastCodepoint(result);
    }
    return result.empty() ? std::string(Ellipsis) : result + std::string(Ellipsis);
}

void fillRoundedRect(Renderer& renderer, UiRect rect, float radius, Color color)
{
    const float r = clamp(radius, 0.0f, std::min(rect.size.x, rect.size.y) * 0.5f);
    if (r <= 0.0f) {
        renderer.fillRect(rect.pos, rect.size, color);
        return;
    }

    const float centerW = std::max(0.0f, rect.size.x - r * 2.0f);
    const float sideH = std::max(0.0f, rect.size.y - r * 2.0f);
    renderer.fillRect({rect.pos.x + r, rect.pos.y}, {centerW, rect.size.y}, color);
    renderer.fillRect({rect.pos.x, rect.pos.y + r}, {r, sideH}, color);
    renderer.fillRect({rect.pos.x + rect.size.x - r, rect.pos.y + r}, {r, sideH}, color);

    renderer.fillCircle({rect.pos.x + r, rect.pos.y + r}, r, color);
    renderer.fillCircle({rect.pos.x + rect.size.x - r, rect.pos.y + r}, r, color);
    renderer.fillCircle({rect.pos.x + r, rect.pos.y + rect.size.y - r}, r, color);
    renderer.fillCircle({rect.pos.x + rect.size.x - r, rect.pos.y + rect.size.y - r}, r, color);
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
        drawUiWindowChrome(renderer, state.panel, state.title, state.helpText, state.cancelButton);
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
    : UiWindowScope(renderer, id, panel, title, helpText, UiWindowOptions{animated, false})
{
}

UiWindowScope::UiWindowScope(
    Renderer& renderer,
    std::string_view id,
    UiRect panel,
    std::string_view title,
    std::string_view helpText,
    UiWindowOptions options)
    : renderer_(&renderer)
{
    if (!options.animated) {
        drawUiWindowChrome(renderer, panel, title, helpText, options.cancelButton);
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
    state.cancelButton = options.cancelButton;
    state.seen = true;
    state.openProgress = std::min(1.0f, state.openProgress + windowAnimationStep);

    const float t = easeOut(state.openProgress);
    applyWindowTransform(renderer, panel, lerp(0.9f, 1.0f, t), t);
    transformed_ = true;
    drawUiWindowChrome(renderer, panel, title, helpText, options.cancelButton);
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

UiCancelControlScope::UiCancelControlScope(UiCancelControlState& state)
    : previous_(activeCancelState)
{
    activeCancelState = &state;
}

UiCancelControlScope::~UiCancelControlScope()
{
    activeCancelState = previous_;
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

UiRect uiCancelButtonRect(UiRect panel)
{
    return {{
        panel.pos.x + panel.size.x - ui::CancelButtonSize.x - ui::CancelButtonOffset.x,
        panel.pos.y + ui::CancelButtonOffset.y,
    }, ui::CancelButtonSize};
}

bool uiCancelRequested(UiCancelControlState& state, const Input& input, UiContext& ui, UiRect panel)
{
    if (input.backPressed()) {
        state.backArmed = true;
    }

    if (!input.backHeld() && !input.backReleased()) {
        state.backArmed = false;
    }

    if (ui.pressed(uiCancelButtonRect(panel))) {
        state.pointerArmed = true;
    }

    if (!input.mouseLeftHeld() && !input.mouseLeftReleased()) {
        state.pointerArmed = false;
    }

    if (input.backReleased() && state.backArmed) {
        state.backArmed = false;
        return true;
    }

    if (input.mouseLeftReleased()) {
        const bool requested = state.pointerArmed && uiCancelButtonRect(panel).contains(ui.mouse());
        state.pointerArmed = false;
        return requested;
    }

    return false;
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
    drawUiWindowChrome(renderer, panel, title, helpText, false);
}

void drawUiCancelButton(Renderer& renderer, UiRect panel)
{
    const UiRect rect = uiCancelButtonRect(panel);
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    const SDL_MouseButtonFlags buttons = SDL_GetMouseState(&mouseX, &mouseY);
    const bool pointerActive = activeCancelState != nullptr &&
        activeCancelState->pointerArmed &&
        (buttons & SDL_BUTTON_LMASK) != 0 &&
        rect.contains({mouseX, mouseY});
    const bool backActive = activeCancelState != nullptr && activeCancelState->backArmed;
    const bool active = pointerActive || backActive;
    const Vec2 drawSize = active ? rect.size * 0.92f : rect.size;
    ImageDrawOptions options;
    options.anchor = {0.5f, 0.5f};
    if (active) {
        options.tint = {210, 210, 210, 235};
    }
    if (renderer.drawImage("assets/UI_cancelButton.png", rect.pos + rect.size * 0.5f, drawSize, options)) {
        return;
    }

    const Vec2 drawPos = rect.pos + (rect.size - drawSize) * 0.5f;
    renderer.fillRect(drawPos, drawSize, active ? Color{22, 18, 34, 220} : Color{30, 24, 42, 230});
    renderer.drawRect(drawPos, drawSize, ui::WindowBorder);
    renderer.drawLine(drawPos + Vec2{15.0f, 16.0f}, drawPos + drawSize - Vec2{15.0f, 16.0f}, ui::Text);
    renderer.drawLine({drawPos.x + drawSize.x - 15.0f, drawPos.y + 16.0f}, {drawPos.x + 15.0f, drawPos.y + drawSize.y - 16.0f}, ui::Text);
}

void drawUiSeparator(Renderer& renderer, UiRect rect, Color tint)
{
    if (rect.size.x <= 0.0f || rect.size.y <= 0.0f) {
        return;
    }

    if (renderer.hasUiLineTexture()) {
        const float y = rect.pos.y + (rect.size.y - ui::SeparatorHeight) * 0.5f;
        renderer.drawUiLine({rect.pos.x, y}, rect.size.x, tint);
        return;
    }

    const float y = rect.pos.y + rect.size.y * 0.5f;
    renderer.drawLine({rect.pos.x, y}, {rect.pos.x + rect.size.x, y}, tint);
}

void drawUiGauge(Renderer& renderer, UiRect rect, float progress, const UiGaugeStyle& style)
{
    if (rect.size.x <= 0.0f || rect.size.y <= 0.0f) {
        return;
    }

    progress = clamp(progress, 0.0f, 1.0f);
    const float radiusLimit = std::min(rect.size.x, rect.size.y) * 0.5f;
    const float radius = style.cornerRadius >= 0.0f
        ? clamp(style.cornerRadius, 0.0f, radiusLimit)
        : radiusLimit;
    const float centerY = rect.pos.y + rect.size.y * 0.5f;
    const Vec2 lineStart{rect.pos.x + radius, centerY};
    const Vec2 lineEnd{rect.pos.x + rect.size.x - radius, centerY};

    if (lineEnd.x <= lineStart.x) {
        const Vec2 center = rect.pos + rect.size * 0.5f;
        if (colorVisible(style.shadow)) {
            renderer.fillSoftCircle(center + Vec2{0.0f, style.shadowOffsetY}, radius + style.shadowExtra * 0.5f, style.shadow);
        }
        if (colorVisible(style.trackOuter)) {
            renderer.fillSoftCircle(center, radius + style.trackOuterExtra * 0.5f, style.trackOuter);
        }
        if (colorVisible(style.track)) {
            renderer.fillCircle(center, radius, style.track);
        }
        if (colorVisible(style.trackInner)) {
            renderer.fillCircle(center, std::max(0.0f, radius - style.trackInnerInset * 0.5f), style.trackInner);
        }
        if (progress > 0.0f) {
            renderer.fillCircle(center, radius, lerpColor(style.fill.start, style.fill.end, 0.5f));
        }
        drawCapsuleOutline(renderer, rect, radius, style.outline);
        return;
    }

    if (colorVisible(style.shadow)) {
        renderer.drawSoftLine(
            lineStart + Vec2{0.0f, style.shadowOffsetY},
            lineEnd + Vec2{0.0f, style.shadowOffsetY},
            std::max(1.0f, rect.size.y + style.shadowExtra),
            style.shadow);
    }
    if (colorVisible(style.trackOuter)) {
        renderer.drawSoftLine(lineStart, lineEnd, std::max(1.0f, rect.size.y + style.trackOuterExtra), style.trackOuter);
    }
    if (colorVisible(style.track)) {
        renderer.drawSoftLine(lineStart, lineEnd, rect.size.y, style.track);
    }
    if (colorVisible(style.trackInner)) {
        const float innerWidth = rect.size.y - style.trackInnerInset;
        if (innerWidth > 0.0f) {
            renderer.drawSoftLine(lineStart, lineEnd, innerWidth, style.trackInner);
        }
    }

    if (style.tickCount > 1 && colorVisible(style.tick)) {
        const float tickHalf = std::max(1.0f, rect.size.y * 0.28f);
        for (int i = 1; i < style.tickCount; ++i) {
            const float x = rect.pos.x + rect.size.x * static_cast<float>(i) / static_cast<float>(style.tickCount);
            renderer.drawLine({x, centerY - tickHalf}, {x, centerY + tickHalf}, style.tick);
        }
    }

    const float filledW = rect.size.x * progress;
    if (filledW <= 0.0f) {
        drawCapsuleOutline(renderer, rect, radius, style.outline);
        return;
    }

    const float fillRight = rect.pos.x + filledW;
    const float fillRadius = std::min(radius, filledW * 0.5f);
    const float fillLeftCenterX = rect.pos.x + fillRadius;
    const float fillRightCenterX = fillRight - fillRadius;
    if (fillRightCenterX > fillLeftCenterX) {
        renderer.fillCircle({fillLeftCenterX, centerY}, fillRadius, style.fill.start);
        renderer.fillGradientRect(
            {fillLeftCenterX, rect.pos.y},
            {fillRightCenterX - fillLeftCenterX, rect.size.y},
            style.fill.start,
            style.fill.end,
            style.fill.direction);
        renderer.fillCircle({fillRightCenterX, centerY}, fillRadius, style.fill.end);
    } else {
        renderer.fillCircle({rect.pos.x + filledW * 0.5f, centerY}, fillRadius, lerpColor(style.fill.start, style.fill.end, 0.5f));
    }

    const float highlightLeft = rect.pos.x + fillRadius;
    const float highlightRight = fillRight - fillRadius;
    if (colorVisible(style.highlight) && highlightRight > highlightLeft + 1.0f) {
        renderer.fillGradientRect(
            {highlightLeft, rect.pos.y + std::max(1.0f, rect.size.y * 0.18f)},
            {highlightRight - highlightLeft, std::max(1.0f, rect.size.y * 0.25f)},
            style.highlight,
            alphaScaledColor(style.highlight, 0.15f),
            GradientDirection::TopToBottom);
    }

    if (style.shimmerPhase >= 0.0f && colorVisible(style.shimmer) && filledW > radius * 2.0f) {
        const float sweepW = std::max(1.0f, style.shimmerWidth);
        const float phase = style.shimmerPhase - std::floor(style.shimmerPhase);
        const float sweepX = rect.pos.x + phase * (rect.size.x + sweepW) - sweepW;
        const float sweepLeft = std::max(rect.pos.x + radius, sweepX);
        const float sweepRight = std::min(fillRight - radius, sweepX + sweepW);
        if (sweepRight > sweepLeft) {
            const Color transparent = alphaScaledColor(style.shimmer, 0.0f);
            renderer.fillGradientRect(
                {sweepLeft, rect.pos.y + rect.size.y * 0.2f},
                {sweepRight - sweepLeft, rect.size.y * 0.6f},
                transparent,
                style.shimmer,
                transparent,
                transparent);
        }
    }

    const Vec2 capCenter{std::clamp(fillRight, lineStart.x, lineEnd.x), centerY};
    if (colorVisible(style.capGlow)) {
        renderer.fillSoftCircle(capCenter, rect.size.y * 0.72f, style.capGlow);
    }
    if (colorVisible(style.capCore)) {
        renderer.fillCircle(capCenter, std::max(1.0f, rect.size.y * 0.14f), style.capCore);
    }
    drawCapsuleOutline(renderer, rect, radius, style.outline);
}

void drawUiButton(Renderer& renderer, UiRect rect, std::string_view label, bool hot, const UiButtonStyle& style)
{
    rect.size.y = ui::ButtonHeight;
    const bool selected = hot;
    const float scale = selected ? 1.035f : 1.0f;
    const Vec2 center = rect.pos + rect.size * 0.5f;
    renderer.pushScreenTransform(center, scale, 1.0f);

    if (renderer.hasUiButtonTexture()) {
        Color tint = selected ? style.imageTintHot : style.imageTint;
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

void drawUiRectButton(Renderer& renderer, UiRect rect, std::string_view label, bool hot, const UiButtonStyle& style)
{
    const bool selected = hot;
    const float scale = selected ? 1.035f : 1.0f;
    const Vec2 center = rect.pos + rect.size * 0.5f;
    renderer.pushScreenTransform(center, scale, 1.0f);

    const Color fill = selected ? style.fillHot : style.fill;
    const Color outline = selected ? scaledColor(style.outlineHot, 1.04f) : style.outline;
    renderer.fillRect(rect.pos, rect.size, fill);
    renderer.drawRect(rect.pos, rect.size, outline);

    const Vec2 textSize = renderer.measureText(label, 2);
    const Vec2 textPos{
        rect.pos.x + std::max(0.0f, (rect.size.x - textSize.x) * 0.5f),
        rect.pos.y + std::max(0.0f, (rect.size.y - textSize.y) * 0.5f),
    };
    renderer.drawText(textPos, label, style.text, 2);
    renderer.popScreenTransform();
}

void drawUiSmallSelectButton(
    Renderer& renderer,
    UiRect rect,
    std::string_view label,
    std::string_view value,
    bool hot,
    bool disabled,
    const UiSmallSelectButtonStyle& style)
{
    const Color fill = disabled ? style.fillDisabled : (hot ? style.fillHot : style.fill);
    const Color outline = disabled ? style.outlineDisabled : (hot ? style.outlineHot : style.outline);
    const Color labelColor = disabled ? style.disabledText : style.text;
    const Color valueColor = disabled ? style.disabledText : style.valueText;
    renderer.fillRect(rect.pos, rect.size, fill);
    renderer.drawRect(rect.pos, rect.size, outline);
    if (hot && !disabled) {
        renderer.fillRect(rect.pos + Vec2{2.0f, 4.0f}, {5.0f, std::max(0.0f, rect.size.y - 8.0f)}, style.accent);
    }

    const int scale = std::max(1, style.textScale);
    constexpr float PaddingX = 12.0f;
    const Vec2 labelSize = renderer.measureText(label, scale);
    const Vec2 valueSize = renderer.measureText(value, scale);
    const float textY = rect.pos.y + std::max(0.0f, (rect.size.y - std::max(labelSize.y, valueSize.y)) * 0.5f);
    renderer.drawText({rect.pos.x + PaddingX, textY}, label, labelColor, scale);
    renderer.drawText({rect.pos.x + rect.size.x - valueSize.x - PaddingX, textY}, value, valueColor, scale);
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

void drawUiDetailLine(Renderer& renderer, UiRect panel, float& y, std::string_view label, std::string_view value, Color valueColor)
{
    constexpr float LabelWidth = 106.0f;
    constexpr float MinLineHeight = 31.0f;
    constexpr float LineGap = 4.0f;
    const float labelX = panel.pos.x + ui::SubPanelPadding.x;
    const float valueX = labelX + LabelWidth;
    const float valueMaxWidth = panel.pos.x + panel.size.x - valueX - ui::SubPanelPadding.x;
    renderer.drawText({labelX, y}, label, ui::TextMuted, 2);
    renderer.drawWrappedText({valueX, y}, value, valueMaxWidth, valueColor, 2);
    y += std::max(MinLineHeight, renderer.measureWrappedText(value, valueMaxWidth, 2).y + LineGap);
}

void openUiCommandMenu(
    UiCommandMenuState& state,
    Vec2 anchor,
    UiRect bounds,
    int itemCount,
    const UiCommandMenuItem* items,
    float minWidth,
    int textScale)
{
    const int effectiveScale = std::max(1, textScale);
    const int count = std::max(1, itemCount);
    int maxCodepoints = 0;
    if (items != nullptr && itemCount > 0) {
        for (int i = 0; i < itemCount; ++i) {
            maxCodepoints = std::max(maxCodepoints, utf8CodepointCount(items[i].label));
        }
    }
    if (maxCodepoints <= 0) {
        maxCodepoints = 6;
    }
    const float estimatedCharWidth = static_cast<float>(effectiveScale) * 8.0f;
    const float contentWidth = static_cast<float>(maxCodepoints) * estimatedCharWidth;
    const float menuWidth = std::max(minWidth, contentWidth + CommandMenuPaddingX * 2.0f + CommandMenuExtraWidth);
    const float menuHeight = CommandMenuPaddingY * 2.0f +
        static_cast<float>(count) * CommandMenuItemHeight +
        static_cast<float>(count - 1) * CommandMenuItemGap;
    Vec2 pos = anchor + Vec2{12.0f, 12.0f};
    const float minX = bounds.pos.x;
    const float minY = bounds.pos.y;
    const float maxX = bounds.pos.x + bounds.size.x - menuWidth;
    const float maxY = bounds.pos.y + bounds.size.y - menuHeight;
    pos.x = clamp(pos.x, minX, std::max(minX, maxX));
    pos.y = clamp(pos.y, minY, std::max(minY, maxY));
    state.open = true;
    state.visible = true;
    state.closing = false;
    state.panel = {pos, {menuWidth, menuHeight}};
    state.textScale = effectiveScale;
    state.hoveredIndex = 0;
    state.animation = 0.0f;
}

void closeUiCommandMenu(UiCommandMenuState& state)
{
    state.open = false;
    state.closing = state.visible;
    if (!state.visible) {
        state.hoveredIndex = -1;
        state.animation = 0.0f;
    }
}

int updateUiCommandMenu(UiCommandMenuState& state, UiContext& ui, const Input& input, const UiCommandMenuItem* items, int itemCount)
{
    const float openStep = clamp(windowAnimationStep * CommandMenuOpenSpeed, 0.0f, 1.0f);
    const float closeStep = clamp(windowAnimationStep * CommandMenuCloseSpeed, 0.0f, 1.0f);
    if (state.closing) {
        state.animation = std::max(0.0f, state.animation - closeStep);
        if (state.animation <= 0.0f) {
            state.visible = false;
            state.closing = false;
            state.hoveredIndex = -1;
        }
        return -1;
    }
    if (!state.visible) {
        return -1;
    }
    state.animation = std::min(1.0f, state.animation + openStep);
    if (!state.open) {
        return -1;
    }
    if (items == nullptr || itemCount <= 0) {
        closeUiCommandMenu(state);
        return -1;
    }

    if (input.pausePressed() || input.pressed(InputAction::OffsetRingCenter)) {
        closeUiCommandMenu(state);
        return -1;
    }

    if (state.hoveredIndex < 0 || state.hoveredIndex >= itemCount) {
        state.hoveredIndex = 0;
    }

    const int delta = (input.pressed(InputAction::MoveDown) || input.pressed(InputAction::MoveRight) ? 1 : 0) -
        (input.pressed(InputAction::MoveUp) || input.pressed(InputAction::MoveLeft) ? 1 : 0);
    if (delta != 0) {
        state.hoveredIndex = (state.hoveredIndex + delta + itemCount) % itemCount;
    }

    bool hoveredByMouse = false;
    for (int i = 0; i < itemCount; ++i) {
        if (commandMenuItemRect(state, i).contains(ui.mouse())) {
            state.hoveredIndex = i;
            hoveredByMouse = true;
            break;
        }
    }
    if (!hoveredByMouse && (state.hoveredIndex < 0 || state.hoveredIndex >= itemCount)) {
        state.hoveredIndex = 0;
    }

    if (input.confirmPressed() || input.useItemPressed()) {
        if (state.hoveredIndex < 0 || state.hoveredIndex >= itemCount || !items[state.hoveredIndex].enabled) {
            return -1;
        }
        const int selected = state.hoveredIndex;
        closeUiCommandMenu(state);
        return selected;
    }

    if (!input.mouseLeftPressed()) {
        return -1;
    }

    const bool insidePanel = state.panel.contains(ui.mouse());
    if (!insidePanel) {
        ui.consumePointer();
        closeUiCommandMenu(state);
        return -1;
    }

    if (state.hoveredIndex < 0 || state.hoveredIndex >= itemCount) {
        return -1;
    }
    ui.consumePointer();
    if (!items[state.hoveredIndex].enabled) {
        return -1;
    }
    const int selected = state.hoveredIndex;
    closeUiCommandMenu(state);
    return selected;
}

void drawUiCommandMenu(Renderer& renderer, const UiCommandMenuState& state, const UiCommandMenuItem* items, int itemCount)
{
    if (!state.visible || items == nullptr || itemCount <= 0) {
        return;
    }

    const float t = easeOut(state.animation);
    const Vec2 center = panelCenter(state.panel);
    renderer.pushScreenTransform(center, lerp(0.92f, 1.0f, t), t);
    drawUiSubPanel(renderer, state.panel);
    const float elapsed = static_cast<float>(SDL_GetTicks()) / 1000.0f;
    const float pulse = 0.5f + 0.5f * std::sin(elapsed * 6.283185307f);
    const unsigned char alpha = static_cast<unsigned char>(std::lround(96.0f + 140.0f * pulse));
    for (int i = 0; i < itemCount; ++i) {
        const UiRect rect = commandMenuItemRect(state, i);
        const bool hot = i == state.hoveredIndex;
        const Color fill = Color{48, 68, 138, alpha};
        Color text = items[i].enabled ? ui::Text : ui::TextDisabled;
        if (!items[i].enabled) {
            text.a = 128;
        }
        if (hot) {
            const UiRect cursorRect{
                {rect.pos.x + 2.0f, rect.pos.y + 2.0f},
                {std::max(0.0f, rect.size.x - 4.0f), std::max(0.0f, rect.size.y - 4.0f)},
            };
            fillRoundedRect(renderer, cursorRect, 8.0f, fill);
        }
        const int textScale = std::max(1, state.textScale);
        const Vec2 textSize = renderer.measureText(items[i].label, textScale);
        const Vec2 textPos{
            rect.pos.x + std::max(0.0f, (rect.size.x - textSize.x) * 0.5f),
            rect.pos.y + std::max(0.0f, (rect.size.y - textSize.y) * 0.5f),
        };
        renderer.drawText(textPos, items[i].label, text, textScale);
    }
    renderer.popScreenTransform();
}

UiRect uiDropdownListRect(UiRect buttonRect, int itemCount, const UiDropdownStyle& style)
{
    const int visibleCount = dropdownVisibleCount(itemCount, style);
    const float rowHeight = std::max(1.0f, style.rowHeight);
    return {
        {buttonRect.pos.x, buttonRect.pos.y + buttonRect.size.y + DropdownGap},
        {buttonRect.size.x, static_cast<float>(visibleCount) * rowHeight + DropdownPadding * 2.0f},
    };
}

UiRect uiDropdownItemRect(UiRect buttonRect, int visibleIndex, const UiDropdownStyle& style)
{
    const UiRect list = uiDropdownListRect(buttonRect, visibleIndex + 1, style);
    const float rowHeight = std::max(1.0f, style.rowHeight);
    return {
        {
            list.pos.x + DropdownPadding,
            list.pos.y + DropdownPadding + static_cast<float>(std::max(0, visibleIndex)) * rowHeight,
        },
        {std::max(0.0f, list.size.x - DropdownPadding * 2.0f), std::max(1.0f, rowHeight - 4.0f)},
    };
}

int updateUiDropdown(
    UiDropdownState& state,
    UiContext& ui,
    const Input& input,
    UiRect buttonRect,
    int selectedIndex,
    const UiDropdownItem* items,
    int itemCount,
    const UiDropdownStyle& style)
{
    const int count = std::max(0, itemCount);
    int selected = -1;

    if (ui.pressed(buttonRect)) {
        state.open = !state.open;
        if (state.open) {
            state.highlightedIndex = count > 0 ? std::clamp(selectedIndex, 0, count - 1) : -1;
            keepDropdownHighlightVisible(state, count, style);
        }
        return -1;
    }

    if (!state.open) {
        return -1;
    }

    clampDropdownState(state, count, selectedIndex, style);
    const UiRect listRect = uiDropdownListRect(buttonRect, count, style);

    if (count > 0) {
        int move = 0;
        if (input.pressed(InputAction::MoveDown) || input.pressed(InputAction::MoveRight)) {
            ++move;
        }
        if (input.pressed(InputAction::MoveUp) || input.pressed(InputAction::MoveLeft)) {
            --move;
        }
        moveDropdownHighlight(state, move, count, style);

        const int wheel = input.shortcutCursorDelta();
        if (wheel != 0 && listRect.contains(ui.mouse())) {
            scrollDropdown(state, wheel, count, style);
        }

        const int visibleCount = dropdownVisibleCount(count, style);
        for (int i = 0; i < visibleCount; ++i) {
            const int itemIndex = state.scrollOffset + i;
            if (itemIndex < 0 || itemIndex >= count) {
                continue;
            }
            const UiRect itemRect = uiDropdownItemRect(buttonRect, i, style);
            UiRect itemHotRect = itemRect;
            itemHotRect.size.x = std::max(0.0f, itemHotRect.size.x - dropdownScrollbarReserve(count, style));
            if (itemHotRect.contains(ui.mouse())) {
                state.highlightedIndex = itemIndex;
            }
            if (ui.pressed(itemHotRect)) {
                if (items != nullptr && items[itemIndex].enabled) {
                    selected = itemIndex;
                    state.open = false;
                }
                return selected;
            }
        }

        if (input.confirmPressed() || input.useItemPressed()) {
            const int index = state.highlightedIndex;
            if (items != nullptr && index >= 0 && index < count && items[index].enabled) {
                selected = index;
                state.open = false;
                return selected;
            }
        }
    }

    if (input.backPressed()) {
        state.open = false;
        return -1;
    }

    if (input.mouseLeftPressed() && !ui.pointerConsumed() && !buttonRect.contains(ui.mouse()) && !listRect.contains(ui.mouse())) {
        ui.consumePointer();
        state.open = false;
        return -1;
    }

    ui.block(listRect);
    return -1;
}

void drawUiDropdown(
    Renderer& renderer,
    const UiDropdownState& state,
    UiRect buttonRect,
    std::string_view selectedLabel,
    const UiDropdownItem* items,
    int itemCount,
    const UiDropdownStyle& style)
{
    UiButtonStyle buttonStyle;
    buttonStyle.fill = style.fill;
    buttonStyle.fillHot = style.fillHot;
    buttonStyle.outline = style.outline;
    buttonStyle.outlineHot = style.outline;
    buttonStyle.text = style.text;
    const int textScale = std::max(1, style.textScale);
    const std::string buttonText = fittedUiText(
        renderer,
        selectedLabel,
        std::max(0.0f, buttonRect.size.x - DropdownTextPaddingX * 2.0f - DropdownArrowWidth),
        textScale);
    drawUiRectButton(renderer, buttonRect, buttonText, state.open, buttonStyle);

    const Vec2 arrowCenter{buttonRect.pos.x + buttonRect.size.x - 17.0f, buttonRect.pos.y + buttonRect.size.y * 0.5f};
    const float arrowY = state.open ? -1.0f : 1.0f;
    renderer.drawLine(arrowCenter + Vec2{-5.0f, -2.0f * arrowY}, arrowCenter + Vec2{0.0f, 4.0f * arrowY}, style.arrow);
    renderer.drawLine(arrowCenter + Vec2{5.0f, -2.0f * arrowY}, arrowCenter + Vec2{0.0f, 4.0f * arrowY}, style.arrow);

    if (!state.open) {
        return;
    }

    const int count = std::max(0, itemCount);
    const UiRect listRect = uiDropdownListRect(buttonRect, count, style);
    renderer.fillRect(listRect.pos, listRect.size, style.fill);
    renderer.drawRect(listRect.pos, listRect.size, style.outline);

    if (items == nullptr || count <= 0) {
        const UiRect row = uiDropdownItemRect(buttonRect, 0, style);
        const Vec2 textSize = renderer.measureText(style.emptyLabel, textScale);
        const Vec2 textPos{
            row.pos.x + DropdownTextPaddingX,
            row.pos.y + std::max(0.0f, (row.size.y - textSize.y) * 0.5f),
        };
        renderer.drawText(textPos, style.emptyLabel, style.textDisabled, textScale);
        return;
    }

    const int visibleCount = dropdownVisibleCount(count, style);
    const float scrollbarReserve = dropdownScrollbarReserve(count, style);
    for (int i = 0; i < visibleCount; ++i) {
        const int itemIndex = state.scrollOffset + i;
        if (itemIndex < 0 || itemIndex >= count) {
            continue;
        }
        const UiRect row = uiDropdownItemRect(buttonRect, i, style);
        UiRect rowBody = row;
        rowBody.size.x = std::max(0.0f, rowBody.size.x - scrollbarReserve);
        const bool highlighted = itemIndex == state.highlightedIndex;
        if (highlighted) {
            renderer.fillRect(rowBody.pos, rowBody.size, style.fillHot);
        }
        const Color textColor = items[itemIndex].enabled ? style.text : style.textDisabled;
        const std::string label = fittedUiText(
            renderer,
            items[itemIndex].label,
            std::max(0.0f, rowBody.size.x - DropdownTextPaddingX * 2.0f),
            textScale);
        const Vec2 textSize = renderer.measureText(label, textScale);
        const Vec2 textPos{
            rowBody.pos.x + DropdownTextPaddingX,
            rowBody.pos.y + std::max(0.0f, (rowBody.size.y - textSize.y) * 0.5f),
        };
        renderer.drawText(textPos, label, textColor, textScale);
    }

    if (dropdownNeedsScrollbar(count, style)) {
        const float trackX = listRect.pos.x + listRect.size.x - DropdownPadding - DropdownScrollbarWidth;
        const float trackY = listRect.pos.y + DropdownScrollbarPaddingY;
        const float trackHeight = std::max(1.0f, listRect.size.y - DropdownScrollbarPaddingY * 2.0f);
        renderer.fillRect({trackX, trackY}, {DropdownScrollbarWidth, trackHeight}, style.scrollbarTrack);

        const float visibleRatio = static_cast<float>(visibleCount) / static_cast<float>(count);
        const float thumbHeight = std::clamp(
            trackHeight * visibleRatio,
            std::min(trackHeight, DropdownScrollbarMinThumbHeight),
            trackHeight);
        const int maxScroll = dropdownMaxScrollOffset(count, style);
        const float scrollRatio = maxScroll > 0 ? static_cast<float>(state.scrollOffset) / static_cast<float>(maxScroll) : 0.0f;
        const float thumbY = trackY + (trackHeight - thumbHeight) * scrollRatio;
        renderer.fillRect({trackX, thumbY}, {DropdownScrollbarWidth, thumbHeight}, style.scrollbarThumb);
    }
}

int updateUiTabs(
    UiTabsState& state,
    UiContext& ui,
    const UiTabsInput& input,
    int selectedIndex,
    const UiTabItem* items,
    int itemCount,
    const UiRect* rects,
    const UiTabsStyle& style)
{
    if (items == nullptr || rects == nullptr || itemCount <= 0) {
        state.focusedIndex = -1;
        return -1;
    }

    clampTabFocus(state, selectedIndex, items, itemCount);

    for (int i = 0; i < itemCount; ++i) {
        if (tabItemEnabled(items, i) && ui.hovered(rects[i])) {
            state.focusedIndex = i;
        }
        if (ui.pressed(rects[i])) {
            if (!tabItemEnabled(items, i)) {
                return -1;
            }
            state.focusedIndex = i;
            return i;
        }
    }

    if (input.directFocusIndex >= 0 && input.directFocusIndex < itemCount && tabItemEnabled(items, input.directFocusIndex)) {
        state.focusedIndex = input.directFocusIndex;
    }
    if (input.focusDelta != 0) {
        moveTabFocus(state, input.focusDelta, items, itemCount, style);
    }

    if (input.commit && state.focusedIndex >= 0 && state.focusedIndex < itemCount &&
        state.focusedIndex != selectedIndex && tabItemEnabled(items, state.focusedIndex)) {
        return state.focusedIndex;
    }

    return -1;
}

void drawUiTabs(
    Renderer& renderer,
    const UiTabsState& state,
    int selectedIndex,
    const UiTabItem* items,
    int itemCount,
    const UiRect* rects,
    const UiTabsStyle& style)
{
    if (items == nullptr || rects == nullptr || itemCount <= 0) {
        return;
    }

    for (int i = 0; i < itemCount; ++i) {
        UiButtonStyle buttonStyle = style.buttonStyle;
        if (i == selectedIndex) {
            buttonStyle.fillHot = style.selectedFillHot;
            buttonStyle.outlineHot = style.selectedOutlineHot;
            buttonStyle.text = style.selectedText;
            buttonStyle.imageTintHot = style.selectedImageTint;
        }
        if (!tabItemEnabled(items, i)) {
            buttonStyle.text = ui::TextDisabled;
        }
        drawUiButton(renderer, rects[i], items[i].label, i == selectedIndex, buttonStyle);
        if (i == state.focusedIndex && i != selectedIndex && tabItemEnabled(items, i) && style.focusOutline.a > 0) {
            UiRect focusRect = rects[i];
            focusRect.size.y = ui::ButtonHeight;
            constexpr float Inset = 3.0f;
            renderer.drawRect(
                focusRect.pos + Vec2{Inset, Inset},
                {
                    std::max(0.0f, focusRect.size.x - Inset * 2.0f),
                    std::max(0.0f, focusRect.size.y - Inset * 2.0f),
                },
                style.focusOutline);
        }
    }
}

}
