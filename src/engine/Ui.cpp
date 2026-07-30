#include "engine/Ui.hpp"

#include "engine/InputHelpGlyph.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
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
    UiWindowFrame frame = UiWindowFrame::Default;
};

UiCancelControlState* activeCancelState = nullptr;
bool backInputConsumedUntilRelease = false;
const std::unordered_map<std::string, float>* menuIconScaleOverrides = nullptr;

std::unordered_map<std::string, UiWindowState> windowStates;

constexpr std::string_view ConfirmDialogHelpText = "F/Enter 決定  Esc 戻る";
constexpr std::string_view UiSelectionCursorPath = "assets/system/UI_cursor2.png";
constexpr std::string_view UiMenuIconDir = "assets/others/";
constexpr std::string_view UiMenuIconPrefix = "img_";
constexpr std::string_view UiMenuIconExtension = ".png";
constexpr Vec2 UiSelectionCursorSize{58.0f, 58.0f};
constexpr Vec2 UiSelectionCursorTargetOffset{8.0f, -5.0f};
constexpr float UiSelectionCursorMoveResponsiveness = 18.0f;
constexpr float UiSelectionCursorBobAmplitude = 3.0f;
constexpr float UiSelectionCursorBobSpeed = 3.2f;
constexpr float UiTextIconGap = 8.0f;
constexpr float UiButtonIconSize = 34.0f;
constexpr float UiTabIconSize = 30.0f;
float windowAnimationStep = 1.0f / ui::WindowAnimationFrames;

struct UiSelectionCursorState {
    bool enabled = false;
    bool targetThisFrame = false;
    bool suppressedThisFrame = false;
    bool hasPosition = false;
    Vec2 target{};
    Vec2 position{};
    float frameDt = 0.0f;
    float time = 0.0f;
};

UiSelectionCursorState selectionCursor;

struct UiNavigationTarget {
    UiRect rect{};
    UiNavigationRole role = UiNavigationRole::Control;
    bool preferred = false;
    bool enabled = true;
    int layer = 0;
};

std::vector<UiNavigationTarget> previousNavigationTargets;
std::vector<UiNavigationTarget> currentNavigationTargets;
UiRect navigationFocusRect{};
UiNavigationRole activeNavigationFocusRole = UiNavigationRole::Control;
bool navigationHasFocus = false;
bool uiNavigationActive = false;
bool navigationWasActive = false;
int navigationLayer = 0;

constexpr float NavigationRectEpsilon = 0.5f;
constexpr float NavigationDirectionEpsilon = 0.5f;

bool navigationRectsMatch(UiRect left, UiRect right)
{
    return std::abs(left.pos.x - right.pos.x) <= NavigationRectEpsilon &&
        std::abs(left.pos.y - right.pos.y) <= NavigationRectEpsilon &&
        std::abs(left.size.x - right.size.x) <= NavigationRectEpsilon &&
        std::abs(left.size.y - right.size.y) <= NavigationRectEpsilon;
}

Vec2 navigationRectCenter(UiRect rect)
{
    return rect.pos + rect.size * 0.5f;
}

bool navigationRectsShareRow(UiRect left, UiRect right)
{
    const float overlapTop = std::max(left.pos.y, right.pos.y);
    const float overlapBottom = std::min(left.pos.y + left.size.y, right.pos.y + right.size.y);
    if (overlapBottom > overlapTop) {
        return true;
    }
    const float centerDistance = std::abs(navigationRectCenter(left).y - navigationRectCenter(right).y);
    return centerDistance <= std::max(left.size.y, right.size.y) * 0.55f;
}

int enabledNavigationTargetIndexForRect(const std::vector<UiNavigationTarget>& targets, UiRect rect)
{
    for (int i = 0; i < static_cast<int>(targets.size()); ++i) {
        if (targets[static_cast<std::size_t>(i)].enabled &&
            navigationRectsMatch(targets[static_cast<std::size_t>(i)].rect, rect)) {
            return i;
        }
    }
    return -1;
}

int preferredNavigationTargetIndex(const std::vector<UiNavigationTarget>& targets)
{
    int preferredIndex = -1;
    int preferredPriority = -1;
    for (int i = 0; i < static_cast<int>(targets.size()); ++i) {
        const UiNavigationTarget& target = targets[static_cast<std::size_t>(i)];
        if (target.enabled && target.preferred) {
            const int priority = target.role == UiNavigationRole::Grid
                ? 2
                : (target.role == UiNavigationRole::Control ? 1 : 0);
            if (priority > preferredPriority) {
                preferredIndex = i;
                preferredPriority = priority;
            }
        }
    }
    if (preferredIndex >= 0) {
        return preferredIndex;
    }
    for (int i = 0; i < static_cast<int>(targets.size()); ++i) {
        if (targets[static_cast<std::size_t>(i)].enabled) {
            return i;
        }
    }
    return -1;
}

void focusNavigationTarget(const UiNavigationTarget& target)
{
    navigationFocusRect = target.rect;
    activeNavigationFocusRole = target.role;
    navigationHasFocus = true;
}

int directionalNavigationTargetIndex(
    const std::vector<UiNavigationTarget>& targets,
    int currentIndex,
    int dx,
    int dy)
{
    if (currentIndex < 0 || currentIndex >= static_cast<int>(targets.size()) || (dx == 0 && dy == 0)) {
        return -1;
    }

    const UiNavigationTarget& current = targets[static_cast<std::size_t>(currentIndex)];
    const Vec2 currentCenter = navigationRectCenter(current.rect);
    int bestIndex = -1;
    float bestScore = std::numeric_limits<float>::max();

    const auto consider = [&](int index, bool requireSameRole, bool requireSameRow) {
        if (index == currentIndex) {
            return;
        }
        const UiNavigationTarget& candidate = targets[static_cast<std::size_t>(index)];
        if (!candidate.enabled ||
            (requireSameRole && candidate.role != current.role) ||
            (requireSameRow && !navigationRectsShareRow(current.rect, candidate.rect))) {
            return;
        }

        const Vec2 candidateCenter = navigationRectCenter(candidate.rect);
        const float deltaX = candidateCenter.x - currentCenter.x;
        const float deltaY = candidateCenter.y - currentCenter.y;
        if ((dx < 0 && deltaX >= -NavigationDirectionEpsilon) ||
            (dx > 0 && deltaX <= NavigationDirectionEpsilon) ||
            (dy < 0 && deltaY >= -NavigationDirectionEpsilon) ||
            (dy > 0 && deltaY <= NavigationDirectionEpsilon)) {
            return;
        }

        const float primary = dx != 0 ? std::abs(deltaX) : std::abs(deltaY);
        const float cross = dx != 0 ? std::abs(deltaY) : std::abs(deltaX);
        const float score = primary + cross * 2.25f;
        if (score < bestScore) {
            bestScore = score;
            bestIndex = index;
        }
    };

    if (dx != 0) {
        for (int i = 0; i < static_cast<int>(targets.size()); ++i) {
            consider(i, current.role == UiNavigationRole::Grid || current.role == UiNavigationRole::Tab, true);
        }
        if (bestIndex >= 0 || current.role != UiNavigationRole::Grid) {
            return bestIndex;
        }

        // グリッドだけは同じ行の左右端を循環する。
        float wrappedX = dx < 0 ? -std::numeric_limits<float>::max() : std::numeric_limits<float>::max();
        for (int i = 0; i < static_cast<int>(targets.size()); ++i) {
            if (i == currentIndex) {
                continue;
            }
            const UiNavigationTarget& candidate = targets[static_cast<std::size_t>(i)];
            if (!candidate.enabled ||
                candidate.role != UiNavigationRole::Grid ||
                !navigationRectsShareRow(current.rect, candidate.rect)) {
                continue;
            }
            const float candidateX = navigationRectCenter(candidate.rect).x;
            const bool better = dx < 0 ? candidateX > wrappedX : candidateX < wrappedX;
            if (better) {
                wrappedX = candidateX;
                bestIndex = i;
            }
        }
        return bestIndex;
    }

    if (current.role == UiNavigationRole::Grid) {
        for (int i = 0; i < static_cast<int>(targets.size()); ++i) {
            consider(i, true, false);
        }
        if (bestIndex >= 0) {
            return bestIndex;
        }
    }

    bestScore = std::numeric_limits<float>::max();
    for (int i = 0; i < static_cast<int>(targets.size()); ++i) {
        consider(i, false, false);
    }
    return bestIndex;
}

void updateUiNavigation(const Input& input)
{
    uiNavigationActive = input.uiNavigationCursorActive();
    if (previousNavigationTargets.empty()) {
        navigationHasFocus = false;
        navigationWasActive = uiNavigationActive;
        return;
    }

    int currentIndex = navigationHasFocus
        ? enabledNavigationTargetIndexForRect(previousNavigationTargets, navigationFocusRect)
        : -1;
    if (currentIndex < 0 || (uiNavigationActive && !navigationWasActive)) {
        currentIndex = preferredNavigationTargetIndex(previousNavigationTargets);
        if (currentIndex >= 0) {
            focusNavigationTarget(previousNavigationTargets[static_cast<std::size_t>(currentIndex)]);
        }
    }

    if (uiNavigationActive && currentIndex >= 0) {
        const int dx =
            (input.pressed(InputAction::MoveRight) ? 1 : 0) -
            (input.pressed(InputAction::MoveLeft) ? 1 : 0);
        const int dy =
            (input.pressed(InputAction::MoveDown) ? 1 : 0) -
            (input.pressed(InputAction::MoveUp) ? 1 : 0);
        const int nextIndex = dy != 0
            ? directionalNavigationTargetIndex(previousNavigationTargets, currentIndex, 0, dy)
            : directionalNavigationTargetIndex(previousNavigationTargets, currentIndex, dx, 0);
        if (nextIndex >= 0 && nextIndex != currentIndex) {
            focusNavigationTarget(previousNavigationTargets[static_cast<std::size_t>(nextIndex)]);
        }
    }
    navigationWasActive = uiNavigationActive;
}

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

bool uiWindowFrameHasImageTexture(Renderer& renderer, UiWindowFrame frame)
{
    if (frame == UiWindowFrame::Message || frame == UiWindowFrame::SystemMessage) {
        const UiMessageWindowKind kind = frame == UiWindowFrame::SystemMessage
            ? UiMessageWindowKind::System
            : UiMessageWindowKind::Speaker;
        return renderer.hasUiMessageWindowTexture(kind);
    }
    return renderer.hasUiWindowTexture();
}

bool uiWindowFrameIsMessage(UiWindowFrame frame)
{
    return frame == UiWindowFrame::Message || frame == UiWindowFrame::SystemMessage;
}

UiMessageWindowKind uiMessageWindowKindForFrame(UiWindowFrame frame)
{
    return frame == UiWindowFrame::SystemMessage
        ? UiMessageWindowKind::System
        : UiMessageWindowKind::Speaker;
}

void drawUiWindowChrome(
    Renderer& renderer,
    UiRect panel,
    std::string_view title,
    std::string_view helpText,
    bool cancelButton,
    UiWindowFrame frame)
{
    drawUiPanel(renderer, panel, frame);
    drawUiHeader(renderer, panel, title, frame);
    drawUiFooter(renderer, panel, helpText, frame);
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

void setUiSelectionCursorTarget(UiRect rect)
{
    if (!selectionCursor.enabled ||
        selectionCursor.suppressedThisFrame ||
        rect.size.x <= 0.0f ||
        rect.size.y <= 0.0f) {
        return;
    }

    selectionCursor.target = {
        rect.pos.x + rect.size.x + UiSelectionCursorTargetOffset.x,
        rect.pos.y + UiSelectionCursorTargetOffset.y,
    };
    selectionCursor.targetThisFrame = true;
}

void drawUiSelectionCursor(Renderer& renderer)
{
    if (!selectionCursor.enabled || !selectionCursor.targetThisFrame) {
        selectionCursor.hasPosition = false;
        return;
    }

    if (!selectionCursor.hasPosition) {
        selectionCursor.position = selectionCursor.target;
        selectionCursor.hasPosition = true;
    } else {
        const float t = 1.0f - std::exp(-UiSelectionCursorMoveResponsiveness * std::max(0.0f, selectionCursor.frameDt));
        selectionCursor.position = selectionCursor.position + (selectionCursor.target - selectionCursor.position) * t;
    }

    const float bob = std::sin(selectionCursor.time * UiSelectionCursorBobSpeed) * UiSelectionCursorBobAmplitude;
    const Vec2 drawCenter = selectionCursor.position + Vec2{0.0f, bob};
    ImageDrawOptions options;
    options.anchor = {0.5f, 0.5f};
    options.tint = {255, 255, 255, 235};
    renderer.drawImage(UiSelectionCursorPath, drawCenter, UiSelectionCursorSize, options, TextureFilter::Linear);
}

bool drawUiFlexibleButtonImage(Renderer& renderer, UiRect rect, bool selected, Color tint)
{
    constexpr std::string_view FlexibleButtonPath = "assets/system/UI_buttons2.png";
    Vec2 imageSize{};
    if (!renderer.getImageSize(FlexibleButtonPath, imageSize, TextureFilter::Nearest) ||
        imageSize.x <= 0.0f || imageSize.y <= 0.0f) {
        return false;
    }

    const ImageHandle handle = renderer.acquireImage(FlexibleButtonPath, TextureFilter::Nearest);
    if (!handle.valid()) {
        return false;
    }

    const float sx = imageSize.x / 300.0f;
    const float sy = imageSize.y / 600.0f;
    const float stateY = selected ? 300.0f * sy : 0.0f;
    const float topHeight = selected ? 123.0f * sy : 120.0f * sy;
    const float centerY = stateY + (selected ? 124.0f * sy : 121.0f * sy);
    const float centerHeight = selected ? 55.0f * sy : 56.0f * sy;
    const float bottomY = stateY + (selected ? 180.0f * sy : 178.0f * sy);
    const float bottomHeight = selected ? 120.0f * sy : 122.0f * sy;
    const float leftWidth = 120.0f * sx;
    const float centerX = 121.0f * sx;
    const float centerWidth = 56.0f * sx;
    const float rightX = 178.0f * sx;
    const float rightWidth = 122.0f * sx;

    const float fixedWidth = leftWidth + rightWidth;
    const float fixedHeight = topHeight + bottomHeight;
    const float scaleX = fixedWidth > 0.0f ? std::min(1.0f, rect.size.x / fixedWidth) : 1.0f;
    const float scaleY = fixedHeight > 0.0f ? std::min(1.0f, rect.size.y / fixedHeight) : 1.0f;
    const float dstLeftWidth = leftWidth * scaleX;
    const float dstRightWidth = rightWidth * scaleX;
    const float dstTopHeight = topHeight * scaleY;
    const float dstBottomHeight = bottomHeight * scaleY;
    const float dstCenterWidth = std::max(0.0f, rect.size.x - dstLeftWidth - dstRightWidth);
    const float dstCenterHeight = std::max(0.0f, rect.size.y - dstTopHeight - dstBottomHeight);
    const float dstRightX = rect.pos.x + rect.size.x - dstRightWidth;
    const float dstBottomY = rect.pos.y + rect.size.y - dstBottomHeight;

    const ImageDrawOptions options{{0.0f, 0.0f}, tint};
    const auto drawCell = [&](RectF source, Vec2 pos, Vec2 size) {
        if (source.w <= 0.0f || source.h <= 0.0f || size.x <= 0.0f || size.y <= 0.0f) {
            return;
        }
        renderer.drawImageRegion(handle, source, pos, size, options);
    };

    drawCell({0.0f, stateY, leftWidth, topHeight}, rect.pos, {dstLeftWidth, dstTopHeight});
    drawCell({centerX, stateY, centerWidth, topHeight}, {rect.pos.x + dstLeftWidth, rect.pos.y}, {dstCenterWidth, dstTopHeight});
    drawCell({rightX, stateY, rightWidth, topHeight}, {dstRightX, rect.pos.y}, {dstRightWidth, dstTopHeight});
    drawCell({0.0f, centerY, leftWidth, centerHeight}, {rect.pos.x, rect.pos.y + dstTopHeight}, {dstLeftWidth, dstCenterHeight});
    drawCell({centerX, centerY, centerWidth, centerHeight}, {rect.pos.x + dstLeftWidth, rect.pos.y + dstTopHeight}, {dstCenterWidth, dstCenterHeight});
    drawCell({rightX, centerY, rightWidth, centerHeight}, {dstRightX, rect.pos.y + dstTopHeight}, {dstRightWidth, dstCenterHeight});
    drawCell({0.0f, bottomY, leftWidth, bottomHeight}, {rect.pos.x, dstBottomY}, {dstLeftWidth, dstBottomHeight});
    drawCell({centerX, bottomY, centerWidth, bottomHeight}, {rect.pos.x + dstLeftWidth, dstBottomY}, {dstCenterWidth, dstBottomHeight});
    drawCell({rightX, bottomY, rightWidth, bottomHeight}, {dstRightX, dstBottomY}, {dstRightWidth, dstBottomHeight});
    return true;
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

std::size_t utf8CodepointByteLength(std::string_view text, std::size_t index)
{
    if (index >= text.size()) {
        return 0;
    }
    const unsigned char lead = static_cast<unsigned char>(text[index]);
    std::size_t length = 1;
    if ((lead & 0x80u) == 0) {
        length = 1;
    } else if ((lead & 0xe0u) == 0xc0u) {
        length = 2;
    } else if ((lead & 0xf0u) == 0xe0u) {
        length = 3;
    } else if ((lead & 0xf8u) == 0xf0u) {
        length = 4;
    }
    return std::min(length, text.size() - index);
}

bool appendUiTextInput(std::string& target, std::string_view text, int maxCodepoints)
{
    if (text.empty()) {
        return false;
    }

    const int limit = std::max(0, maxCodepoints);
    int count = utf8CodepointCount(target);
    bool changed = false;
    for (std::size_t i = 0; i < text.size();) {
        const unsigned char lead = static_cast<unsigned char>(text[i]);
        const std::size_t length = utf8CodepointByteLength(text, i);
        if (length == 0) {
            break;
        }
        if (lead < 0x20u || lead == 0x7fu) {
            i += length;
            continue;
        }
        if (limit > 0 && count >= limit) {
            break;
        }
        target.append(text.substr(i, length));
        ++count;
        changed = true;
        i += length;
    }
    return changed;
}

bool uiTextInputShouldConsumeKey(SDL_Scancode scancode)
{
    if (scancode >= SDL_SCANCODE_F1 && scancode <= SDL_SCANCODE_F24) {
        return false;
    }
    switch (scancode) {
    case SDL_SCANCODE_ESCAPE:
    case SDL_SCANCODE_RETURN:
    case SDL_SCANCODE_KP_ENTER:
    case SDL_SCANCODE_UP:
    case SDL_SCANCODE_DOWN:
    case SDL_SCANCODE_LEFT:
    case SDL_SCANCODE_RIGHT:
    case SDL_SCANCODE_PAGEUP:
    case SDL_SCANCODE_PAGEDOWN:
    case SDL_SCANCODE_HOME:
    case SDL_SCANCODE_END:
    case SDL_SCANCODE_LCTRL:
    case SDL_SCANCODE_RCTRL:
    case SDL_SCANCODE_LSHIFT:
    case SDL_SCANCODE_RSHIFT:
    case SDL_SCANCODE_LALT:
    case SDL_SCANCODE_RALT:
    case SDL_SCANCODE_LGUI:
    case SDL_SCANCODE_RGUI:
        return false;
    default:
        return true;
    }
}

void syncUiTextInputActive(UiTextInputState& state, UiRect* area = nullptr)
{
    SDL_Window* window = SDL_GetKeyboardFocus();
    if (window == nullptr) {
        state.textInputActive = false;
        return;
    }
    if (!SDL_TextInputActive(window)) {
        state.textInputActive = SDL_StartTextInput(window);
    } else {
        state.textInputActive = true;
    }
    if (state.textInputActive && area != nullptr) {
        const SDL_Rect textArea{
            static_cast<int>(std::lround(area->pos.x)),
            static_cast<int>(std::lround(area->pos.y)),
            static_cast<int>(std::lround(std::max(1.0f, area->size.x))),
            static_cast<int>(std::lround(std::max(1.0f, area->size.y))),
        };
        SDL_SetTextInputArea(window, &textArea, 0);
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

std::string uiMenuIconPath(int imageNumber)
{
    if (imageNumber <= 0) {
        return {};
    }

    return std::string(UiMenuIconDir) +
        std::string(UiMenuIconPrefix) +
        std::to_string(imageNumber) +
        std::string(UiMenuIconExtension);
}

std::string_view uiMenuIconScaleKey(int imageNumber)
{
    switch (imageNumber) {
    case 25: return "ui_screen_settings";
    case 26: return "ui_volume";
    case 27: return "ui_gamepad";
    case 28: return "ui_status";
    case 29: return "ui_backpack";
    case 30: return "ui_options";
    case 31: return "ui_quit_game";
    case 32: return "ui_storage_chest";
    case 33: return "ui_ring_0";
    case 34: return "ui_ring_8";
    case 35: return "ui_ring_c";
    default: return {};
    }
}

float uiMenuIconScale(int imageNumber)
{
    if (menuIconScaleOverrides == nullptr || menuIconScaleOverrides->empty()) {
        return 1.0f;
    }

    const std::string_view key = uiMenuIconScaleKey(imageNumber);
    if (key.empty()) {
        return 1.0f;
    }

    const auto it = menuIconScaleOverrides->find(std::string(key));
    if (it == menuIconScaleOverrides->end()) {
        return 1.0f;
    }
    return std::max(0.05f, it->second);
}

void drawUiIconImage(Renderer& renderer, int imageNumber, Vec2 center, float size, Color tint)
{
    if (imageNumber <= 0 || size <= 0.0f) {
        return;
    }

    ImageDrawOptions options;
    options.tint = tint;
    renderer.drawImage(uiMenuIconPath(imageNumber), center, {size, size}, options, TextureFilter::Linear);
}

void drawUiLabelWithOptionalIcon(
    Renderer& renderer,
    UiRect rect,
    std::string_view label,
    int iconImageNumber,
    Color textColor,
    int textScale,
    float iconSize,
    float textOffsetY = 0.0f,
    Color iconTint = {255, 255, 255, 255})
{
    const bool hasIcon = iconImageNumber > 0;
    const float scaledIconSize = hasIcon ? iconSize * uiMenuIconScale(iconImageNumber) : 0.0f;
    const float iconSlotSize = hasIcon ? std::max(iconSize, scaledIconSize) : 0.0f;
    const float sidePadding = hasIcon ? std::max(10.0f, iconSlotSize * 0.32f) : 0.0f;
    const float iconGap = hasIcon ? UiTextIconGap : 0.0f;
    const float maxTextWidth = std::max(1.0f, rect.size.x - sidePadding * 2.0f - (hasIcon ? iconSlotSize + iconGap : 0.0f));
    const std::string fitted = fittedUiText(renderer, label, maxTextWidth, textScale);
    const Vec2 textSize = renderer.measureText(fitted, textScale);
    const float groupWidth = textSize.x + (hasIcon ? iconSlotSize + iconGap : 0.0f);
    const float centeredGroupX = rect.pos.x + std::max(sidePadding, (rect.size.x - groupWidth) * 0.5f);
    float groupX = centeredGroupX;
    if (hasIcon && rect.size.x >= 160.0f) {
        const float alignedTextX = rect.pos.x + rect.size.x * 0.5f - std::min(26.0f, rect.size.x * 0.08f);
        const float alignedGroupX = alignedTextX - iconSlotSize - iconGap;
        const float maxGroupX = rect.pos.x + rect.size.x - sidePadding - groupWidth;
        groupX = std::clamp(alignedGroupX, rect.pos.x + sidePadding, std::max(rect.pos.x + sidePadding, maxGroupX));
    }
    const float centerY = rect.pos.y + rect.size.y * 0.5f;
    const float textX = groupX + (hasIcon ? iconSlotSize + iconGap : 0.0f);
    const Vec2 textPos{
        textX,
        rect.pos.y + std::max(0.0f, (rect.size.y - textSize.y) * 0.5f) + textOffsetY,
    };
    if (hasIcon) {
        drawUiIconImage(renderer, iconImageNumber, {groupX + iconSlotSize * 0.5f, centerY}, scaledIconSize, iconTint);
    }
    renderer.drawText(textPos, fitted, textColor, textScale);
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

UiNavigationLayerScope::UiNavigationLayerScope()
    : previousLayer_(navigationLayer)
{
    ++navigationLayer;
}

UiNavigationLayerScope::~UiNavigationLayerScope()
{
    navigationLayer = previousLayer_;
}

void registerUiNavigationTarget(UiRect rect, UiNavigationRole role, bool preferred, bool enabled)
{
    if (rect.size.x <= 0.0f || rect.size.y <= 0.0f) {
        return;
    }
    for (UiNavigationTarget& target : currentNavigationTargets) {
        if (target.layer == navigationLayer &&
            target.role == role &&
            navigationRectsMatch(target.rect, rect)) {
            target.preferred = target.preferred || preferred;
            target.enabled = target.enabled && enabled;
            return;
        }
    }
    currentNavigationTargets.push_back(UiNavigationTarget{
        .rect = rect,
        .role = role,
        .preferred = preferred,
        .enabled = enabled,
        .layer = navigationLayer,
    });
}

void requestUiSelectionCursor(UiRect rect)
{
    setUiSelectionCursorTarget(rect);
}

void suppressUiSelectionCursor()
{
    selectionCursor.suppressedThisFrame = true;
    selectionCursor.targetThisFrame = false;
}

int moveUiGridSelection(int selectedIndex, int itemCount, int columns, int dx, int dy)
{
    if (itemCount <= 0) {
        return 0;
    }
    columns = std::max(1, columns);
    selectedIndex = std::clamp(selectedIndex, 0, itemCount - 1);

    if (dx != 0) {
        const int rowStart = (selectedIndex / columns) * columns;
        const int rowEnd = std::min(itemCount, rowStart + columns) - 1;
        const int rowLength = rowEnd - rowStart + 1;
        const int rowOffset = selectedIndex - rowStart;
        int nextOffset = (rowOffset + dx) % rowLength;
        if (nextOffset < 0) {
            nextOffset += rowLength;
        }
        selectedIndex = rowStart + nextOffset;
    }

    if (dy != 0) {
        const int column = selectedIndex % columns;
        const int targetRow = selectedIndex / columns + dy;
        const int rowCount = (itemCount + columns - 1) / columns;
        if (targetRow >= 0 && targetRow < rowCount) {
            const int targetRowStart = targetRow * columns;
            selectedIndex = std::min(itemCount - 1, targetRowStart + column);
        }
    }
    return selectedIndex;
}

void setUiMenuIconScaleOverrides(const std::unordered_map<std::string, float>* scaleByIconKey)
{
    menuIconScaleOverrides = scaleByIconKey;
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
    , pointerActive_(input.lastInputModality() == InputModality::Mouse)
    , navigationConfirmPressed_(input.confirmPressed() || input.useItemPressed())
{
    updateUiNavigation(input);
    if (!input.backHeld() && !input.backReleased()) {
        backInputConsumedUntilRelease = false;
    }
}

bool UiContext::backInputConsumed() const
{
    return backInputConsumedUntilRelease;
}

void UiContext::consumeBackInput()
{
    backInputConsumedUntilRelease = true;
}

void UiContext::emitSound(UiSoundEvent event)
{
    const int index = static_cast<int>(event);
    if (index < 0 || index >= static_cast<int>(UiSoundEvent::Count)) {
        return;
    }
    ++soundEventCounts_[index];
}

void UiContext::emitCursorMoveIfChanged(int previousIndex, int currentIndex)
{
    if (previousIndex != currentIndex) {
        emitSound(UiSoundEvent::CursorMove);
    }
}

int UiContext::soundEventCount(UiSoundEvent event) const
{
    const int index = static_cast<int>(event);
    if (index < 0 || index >= static_cast<int>(UiSoundEvent::Count)) {
        return 0;
    }
    return soundEventCounts_[index];
}

bool UiContext::hasSoundEvents() const
{
    for (int count : soundEventCounts_) {
        if (count > 0) {
            return true;
        }
    }
    return false;
}

void beginUiFrame(float dt, bool navigationCursorEnabled)
{
    selectionCursor.enabled = navigationCursorEnabled;
    selectionCursor.targetThisFrame = false;
    selectionCursor.suppressedThisFrame = false;
    selectionCursor.frameDt = std::max(0.0f, dt);
    selectionCursor.time += selectionCursor.frameDt;
    const float duration = std::max(0.001f, ui::WindowAnimationSeconds);
    windowAnimationStep = clamp(dt / duration, 0.0f, 1.0f);
    for (auto& entry : windowStates) {
        entry.second.seen = false;
    }
    currentNavigationTargets.clear();
    navigationLayer = 0;
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
        drawUiWindowChrome(renderer, state.panel, state.title, state.helpText, state.cancelButton, state.frame);
        renderer.popScreenTransform();
        if (state.closeProgress >= 1.0f) {
            finished.push_back(entry.first);
        }
    }
    for (const std::string& key : finished) {
        windowStates.erase(key);
    }

    int topLayer = 0;
    for (const UiNavigationTarget& target : currentNavigationTargets) {
        topLayer = std::max(topLayer, target.layer);
    }
    std::vector<UiNavigationTarget> activeTargets;
    activeTargets.reserve(currentNavigationTargets.size());
    for (const UiNavigationTarget& target : currentNavigationTargets) {
        if (target.layer == topLayer) {
            activeTargets.push_back(target);
        }
    }
    int focusedIndex = navigationHasFocus
        ? enabledNavigationTargetIndexForRect(activeTargets, navigationFocusRect)
        : -1;
    if (focusedIndex < 0) {
        focusedIndex = preferredNavigationTargetIndex(activeTargets);
        if (focusedIndex >= 0) {
            focusNavigationTarget(activeTargets[static_cast<std::size_t>(focusedIndex)]);
        } else {
            navigationHasFocus = false;
        }
    }
    previousNavigationTargets = std::move(activeTargets);
    if (selectionCursor.enabled &&
        !selectionCursor.suppressedThisFrame &&
        focusedIndex >= 0) {
        setUiSelectionCursorTarget(previousNavigationTargets[static_cast<std::size_t>(focusedIndex)].rect);
    }
    drawUiSelectionCursor(renderer);
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
        drawUiWindowChrome(renderer, panel, title, helpText, options.cancelButton, options.frame);
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
    state.frame = options.frame;
    state.seen = true;
    state.openProgress = std::min(1.0f, state.openProgress + windowAnimationStep);

    const float t = easeOut(state.openProgress);
    applyWindowTransform(renderer, panel, lerp(0.9f, 1.0f, t), t);
    transformed_ = true;
    drawUiWindowChrome(renderer, panel, title, helpText, options.cancelButton, options.frame);
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

bool UiContext::pointerInside(UiRect rect) const
{
    return rect.contains(mouse_);
}

bool UiContext::hovered(UiRect rect) const
{
    return pointerActive_ && !pointerConsumed_ && pointerInside(rect);
}

bool UiContext::pressed(UiRect rect)
{
    const bool hit = hovered(rect);
    if (hit && mouseLeftPressed_) {
        pointerConsumed_ = true;
        navigationFocusRect = rect;
        const int targetIndex = enabledNavigationTargetIndexForRect(previousNavigationTargets, rect);
        activeNavigationFocusRole = targetIndex >= 0
            ? previousNavigationTargets[static_cast<std::size_t>(targetIndex)].role
            : UiNavigationRole::Control;
        navigationHasFocus = true;
        return true;
    }
    if (uiNavigationActive &&
        navigationConfirmPressed_ &&
        !navigationConfirmConsumed_ &&
        navigationFocused(rect)) {
        navigationConfirmConsumed_ = true;
        return true;
    }
    return false;
}

bool UiContext::navigationActive() const
{
    return uiNavigationActive;
}

bool UiContext::navigationFocused(UiRect rect) const
{
    return uiNavigationActive && navigationHasFocus && navigationRectsMatch(navigationFocusRect, rect);
}

bool UiContext::selectionFocused(UiRect rect) const
{
    return navigationFocused(rect) || hovered(rect);
}

UiNavigationRole UiContext::navigationFocusRole() const
{
    return activeNavigationFocusRole;
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

UiRect uiBodyRect(UiRect panel, float bottomExtension, float topExtension)
{
    const float y = panel.pos.y + ui::HeaderHeight;
    const float footerHeight = ui::FooterMaxHeight;
    const Vec2 baseBodyPos{panel.pos.x + ui::PanelPadding, y + ui::PanelPadding};
    const float appliedTopExtension = std::clamp(
        topExtension,
        0.0f,
        std::max(0.0f, baseBodyPos.y - panel.pos.y));
    const Vec2 bodyPos{baseBodyPos.x, baseBodyPos.y - appliedTopExtension};
    const float baseHeight =
        panel.size.y - ui::HeaderHeight - footerHeight - ui::PanelPadding * 2.0f;
    const float maxHeight = std::max(0.0f, panel.pos.y + panel.size.y - bodyPos.y);
    return {
        bodyPos,
        {
            panel.size.x - ui::PanelPadding * 2.0f,
            std::clamp(
                baseHeight + bottomExtension + appliedTopExtension,
                0.0f,
                maxHeight),
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

UiRect uiBottomLeftButtonRect(UiRect panel, Vec2 size, float bodyBottomExtension)
{
    const UiRect body = uiBodyRect(panel, bodyBottomExtension);
    size.y = ui::ButtonHeight;
    return {{body.pos.x, body.pos.y + body.size.y - size.y}, size};
}

UiRect uiBottomCenterButtonRect(UiRect panel, Vec2 size, float bodyBottomExtension)
{
    const UiRect body = uiBodyRect(panel, bodyBottomExtension);
    size.y = ui::ButtonHeight;
    return {{panel.pos.x + (panel.size.x - size.x) * 0.5f, body.pos.y + body.size.y - size.y}, size};
}

UiRect uiBottomRightButtonRect(UiRect panel, Vec2 size, float bodyBottomExtension)
{
    const UiRect body = uiBodyRect(panel, bodyBottomExtension);
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
    if (ui.backInputConsumed()) {
        state.backArmed = false;
    } else if (input.backPressed()) {
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
        ui.consumeBackInput();
        ui.emitSound(UiSoundEvent::Cancel);
        return true;
    }

    if (input.mouseLeftReleased()) {
        const bool requested = state.pointerArmed && uiCancelButtonRect(panel).contains(ui.mouse());
        state.pointerArmed = false;
        if (requested) {
            ui.emitSound(UiSoundEvent::Cancel);
        }
        return requested;
    }

    return false;
}

void drawUiPanel(Renderer& renderer, UiRect panel, UiWindowFrame frame)
{
    if (uiWindowFrameIsMessage(frame)) {
        const UiMessageWindowKind kind = uiMessageWindowKindForFrame(frame);
        if (renderer.hasUiMessageWindowTexture(kind)) {
            renderer.drawUiMessageWindowFrame(panel.pos, panel.size, kind);
            return;
        }
    }
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

void drawUiHeader(Renderer& renderer, UiRect panel, std::string_view title, UiWindowFrame frame)
{
    const UiRect header = uiHeaderRect(panel);
    Vec2 titlePadding = ui::HeaderTitlePadding;
    if (!uiWindowFrameHasImageTexture(renderer, frame)) {
        renderer.fillRect(header.pos, header.size, ui::HeaderFill);
    } else {
        titlePadding = ui::ImageWindowHeaderTitlePadding;
    }
    renderer.drawText(header.pos + titlePadding, title, ui::Text, 3);
    renderer.drawText(header.pos + titlePadding + Vec2{1.0f, 0.0f}, title, ui::Text, 3);
}

void drawUiFooter(Renderer& renderer, UiRect panel, std::string_view helpText, UiWindowFrame frame)
{
    if (helpText.empty()) {
        return;
    }
    const UiRect footer = uiFooterRect(panel, helpText);
    Vec2 textPadding = ui::FooterTextPadding;
    if (!uiWindowFrameHasImageTexture(renderer, frame)) {
        renderer.fillRect(footer.pos, footer.size, ui::FooterFill);
    } else {
        textPadding = ui::ImageWindowFooterTextPadding;
        if (uiWindowFrameIsMessage(frame) &&
            renderer.hasUiMessageWindowTexture(uiMessageWindowKindForFrame(frame))) {
            textPadding.x = std::max(0.0f, textPadding.x + 50.0f);
        }
    }
    InputHelpStyle helpStyle;
    helpStyle.text = ui::TextMuted;
    helpStyle.scale = 2;
    helpStyle.iconHeight = 23.0f;
    const float maxWidth = footer.size.x - textPadding.x * 2.0f;
    const std::string fitted = fittedInputHelpText(renderer, std::string(helpText), maxWidth, helpStyle);
    drawInputHelpText(renderer, footer.pos + textPadding, fitted, helpStyle);
}

void drawUiWindow(Renderer& renderer, UiRect panel, std::string_view title, std::string_view helpText)
{
    drawUiWindowChrome(renderer, panel, title, helpText, false, UiWindowFrame::Default);
}

void drawUiModalBackdrop(Renderer& renderer, UiRect bounds, Color color)
{
    renderer.fillRect(bounds.pos, bounds.size, color);
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
    if (renderer.drawImage("assets/system/UI_cancelButton.png", rect.pos + rect.size * 0.5f, drawSize, options)) {
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

UiSliderResult updateUiSlider(
    UiContext& ui,
    const Input& input,
    UiRect rect,
    float value,
    const UiSliderSpec& spec,
    UiSliderState& state)
{
    UiSliderResult result{value, false};
    const float range = spec.maxValue - spec.minValue;
    if (range <= 0.0f || rect.size.x <= 0.0f || rect.size.y <= 0.0f) {
        state.dragging = false;
        return result;
    }

    const auto valueAtPointer = [&] {
        const float normalized =
            clamp((ui.mouse().x - rect.pos.x) / std::max(1.0f, rect.size.x), 0.0f, 1.0f);
        float nextValue = spec.minValue + range * normalized;
        if (spec.step > 0.0f) {
            const float stepCount = std::round((nextValue - spec.minValue) / spec.step);
            nextValue = spec.minValue + stepCount * spec.step;
        }
        return clamp(nextValue, spec.minValue, spec.maxValue);
    };

    bool interacting = false;
    if (state.dragging) {
        if (input.mouseLeftHeld()) {
            interacting = true;
        } else {
            state.dragging = false;
        }
    } else if (
        input.mouseLeftPressed() &&
        !ui.pointerConsumed() &&
        ui.pointerInside(rect)) {
        state.dragging = true;
        interacting = true;
    }

    if (interacting) {
        result.value = valueAtPointer();
        result.changed = std::abs(result.value - value) > 0.0001f;
        ui.consumePointer();
    }
    return result;
}

void drawUiSlider(
    Renderer& renderer,
    UiRect rect,
    float value,
    const UiSliderSpec& spec,
    const UiSliderStyle& style)
{
    const float range = spec.maxValue - spec.minValue;
    if (range <= 0.0f || rect.size.x <= 0.0f || rect.size.y <= 0.0f) {
        return;
    }

    UiGaugeStyle gaugeStyle = style.gauge;
    if (spec.step > 0.0f) {
        gaugeStyle.tickCount = std::max(
            1,
            static_cast<int>(std::lround(range / spec.step)));
    }
    const float progress = clamp((value - spec.minValue) / range, 0.0f, 1.0f);
    drawUiGauge(renderer, rect, progress, gaugeStyle);

    const float centerY = rect.pos.y + rect.size.y * 0.5f;
    const auto drawTick = [&](float tickValue, float halfHeight, Color color) {
        if (!colorVisible(color) ||
            tickValue <= spec.minValue ||
            tickValue >= spec.maxValue) {
            return;
        }
        const float normalized = (tickValue - spec.minValue) / range;
        const float x = rect.pos.x + rect.size.x * normalized;
        renderer.drawLine({x, centerY - halfHeight}, {x, centerY + halfHeight}, color);
    };

    if (spec.majorTickStep > 0.0f) {
        const float firstTick =
            std::ceil(spec.minValue / spec.majorTickStep) * spec.majorTickStep;
        const float halfHeight = std::max(1.0f, rect.size.y * 0.42f);
        for (float tickValue = firstTick;
             tickValue < spec.maxValue;
             tickValue += spec.majorTickStep) {
            drawTick(tickValue, halfHeight, style.majorTick);
        }
    }
    if (spec.showReference) {
        drawTick(
            spec.referenceValue,
            std::max(1.0f, rect.size.y * 0.68f),
            style.referenceTick);
    }
}

void drawUiButton(Renderer& renderer, UiRect rect, std::string_view label, bool hot, const UiButtonStyle& style)
{
    drawUiButton(renderer, rect, label, 0, hot, style);
}

void drawUiButton(Renderer& renderer, UiRect rect, std::string_view label, int iconImageNumber, bool hot, const UiButtonStyle& style)
{
    rect.size.y = ui::ButtonHeight;
    registerUiNavigationTarget(rect, UiNavigationRole::Control, hot);
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

    drawUiLabelWithOptionalIcon(renderer, rect, label, iconImageNumber, style.text, 2, UiButtonIconSize);
    renderer.popScreenTransform();
    if (selected) {
        requestUiSelectionCursor(rect);
    }
}

void drawUiFlexibleButtonFrame(Renderer& renderer, UiRect rect, bool selected, const UiButtonStyle& style)
{
    registerUiNavigationTarget(rect, UiNavigationRole::Control, selected);
    const Color tint = selected ? style.imageTintHot : style.imageTint;
    if (drawUiFlexibleButtonImage(renderer, rect, selected, tint)) {
        return;
    }

    const Color fill = selected ? style.fillHot : style.fill;
    const Color outline = selected ? scaledColor(style.outlineHot, 1.04f) : style.outline;
    renderer.fillRect(rect.pos, rect.size, fill);
    renderer.drawRect(rect.pos, rect.size, outline);
}

void drawUiFlexibleButton(Renderer& renderer, UiRect rect, std::string_view label, bool selected, const UiButtonStyle& style)
{
    drawUiFlexibleButtonFrame(renderer, rect, selected, style);

    const Vec2 textSize = renderer.measureText(label, 2);
    const Vec2 textPos{
        rect.pos.x + std::max(0.0f, (rect.size.x - textSize.x) * 0.5f),
        rect.pos.y + std::max(0.0f, (rect.size.y - textSize.y) * 0.5f),
    };
    renderer.drawText(textPos, label, style.text, 2);
    if (selected) {
        requestUiSelectionCursor(rect);
    }
}

void drawUiRectButton(Renderer& renderer, UiRect rect, std::string_view label, bool hot, const UiButtonStyle& style)
{
    registerUiNavigationTarget(rect, UiNavigationRole::Control, hot);
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
    if (selected) {
        requestUiSelectionCursor(rect);
    }
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
    registerUiNavigationTarget(rect, UiNavigationRole::Control, hot, !disabled);
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
    const int valueScale = std::max(1, style.valueTextScale);
    constexpr float PaddingX = 12.0f;
    const Vec2 labelSize = renderer.measureText(label, scale);
    const Vec2 valueSize = renderer.measureText(value, valueScale);
    const float textY = rect.pos.y + std::max(0.0f, (rect.size.y - std::max(labelSize.y, valueSize.y)) * 0.5f);
    const float valueY = rect.pos.y + std::max(0.0f, (rect.size.y - valueSize.y) * 0.5f);
    renderer.drawText({rect.pos.x + PaddingX, textY}, label, labelColor, scale);
    renderer.drawText({rect.pos.x + rect.size.x - valueSize.x - PaddingX, valueY}, value, valueColor, valueScale);
}

void focusUiTextInput(UiTextInputState& state)
{
    state.focused = true;
    syncUiTextInputActive(state);
}

void blurUiTextInput(UiTextInputState& state)
{
    if (state.textInputActive) {
        if (SDL_Window* window = SDL_GetKeyboardFocus()) {
            SDL_StopTextInput(window);
        }
    }
    state.composition.clear();
    state.focused = false;
    state.textInputActive = false;
}

bool handleUiTextInputEvent(UiTextInputState& state, const SDL_Event& event, int maxCodepoints)
{
    if (!state.focused) {
        return false;
    }

    if (event.type == SDL_EVENT_TEXT_INPUT) {
        appendUiTextInput(state.text, event.text.text == nullptr ? std::string_view{} : std::string_view(event.text.text), maxCodepoints);
        state.composition.clear();
        return true;
    }

    if (event.type == SDL_EVENT_TEXT_EDITING) {
        state.composition = event.edit.text == nullptr ? std::string{} : std::string(event.edit.text);
        return true;
    }

    if (event.type != SDL_EVENT_KEY_DOWN) {
        return false;
    }

    const SDL_Keymod mods = SDL_GetModState();
    const bool ctrlDown = (mods & SDL_KMOD_CTRL) != 0;
    if (event.key.scancode == SDL_SCANCODE_BACKSPACE) {
        if (state.composition.empty()) {
            removeUtf8LastCodepoint(state.text);
        }
        return true;
    }
    if (event.key.scancode == SDL_SCANCODE_DELETE) {
        if (state.composition.empty() && !state.text.empty()) {
            state.text.clear();
        }
        return true;
    }
    if (ctrlDown && event.key.scancode == SDL_SCANCODE_V) {
        if (char* clipboard = SDL_GetClipboardText()) {
            appendUiTextInput(state.text, clipboard, maxCodepoints);
            SDL_free(clipboard);
        }
        state.composition.clear();
        return true;
    }
    if (ctrlDown) {
        return false;
    }

    return uiTextInputShouldConsumeKey(event.key.scancode);
}

UiTextInputResult updateUiTextInput(UiTextInputState& state, UiContext& ui, UiRect rect)
{
    UiTextInputResult result;
    if (ui.pressed(rect)) {
        const bool wasFocused = state.focused;
        focusUiTextInput(state);
        result.focusedChanged = !wasFocused;
        ui.consumePointer();
    }
    if (state.focused) {
        syncUiTextInputActive(state, &rect);
    }
    return result;
}

void drawUiTextInput(
    Renderer& renderer,
    UiRect rect,
    const UiTextInputState& state,
    std::string_view placeholder,
    const UiTextInputStyle& style)
{
    const Color fill = state.focused ? style.fillFocused : style.fill;
    const Color outline = state.focused ? style.outlineFocused : style.outline;
    renderer.fillRect(rect.pos, rect.size, fill);
    renderer.drawRect(rect.pos, rect.size, outline);

    const int scale = std::max(1, style.textScale);
    const UiRect textRect{
        rect.pos + Vec2{std::max(0.0f, style.padding.x), 0.0f},
        {
            std::max(0.0f, rect.size.x - std::max(0.0f, style.padding.x) * 2.0f),
            rect.size.y,
        },
    };
    const std::string displayValue = state.text + state.composition;
    const bool showPlaceholder = displayValue.empty();
    const std::string text = fittedUiText(
        renderer,
        showPlaceholder ? placeholder : std::string_view(displayValue),
        textRect.size.x,
        scale);
    const Vec2 textSize = renderer.measureText(text, scale);
    const Vec2 textPos{
        textRect.pos.x,
        rect.pos.y + std::max(0.0f, (rect.size.y - textSize.y) * 0.5f),
    };
    renderer.pushClipRect(textRect.pos, textRect.size);
    renderer.drawText(textPos, text, showPlaceholder ? style.placeholder : style.text, scale);
    if (state.focused && !showPlaceholder) {
        const float elapsed = static_cast<float>(SDL_GetTicks()) / 1000.0f;
        if (std::fmod(elapsed, 1.0f) < 0.55f) {
            const float caretX = std::min(textRect.pos.x + textRect.size.x, textPos.x + textSize.x + 3.0f);
            renderer.fillRect({caretX, rect.pos.y + 8.0f}, {2.0f, std::max(4.0f, rect.size.y - 16.0f)}, style.caret);
        }
    }
    renderer.popClipRect();
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

void drawUiSystemMessage(Renderer& renderer, std::string_view message, Vec2 pos, const UiSystemMessageStyle& style)
{
    if (message.empty()) {
        return;
    }

    const int textScale = std::max(1, style.textScale);
    const Vec2 textSize = style.maxWidth > 0.0f
        ? renderer.measureWrappedText(message, style.maxWidth, textScale)
        : renderer.measureText(message, textScale);
    if (style.fill.a > 0) {
        renderer.fillRect(pos - style.padding, textSize + style.padding * 2.0f, style.fill);
    }
    if (style.maxWidth > 0.0f) {
        renderer.drawWrappedText(pos, message, style.maxWidth, style.text, textScale);
    } else {
        renderer.drawText(pos, message, style.text, textScale);
    }
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
    drawUiDetailText(renderer, panel, y, text, ui::Text);
}

void drawUiDetailText(Renderer& renderer, UiRect panel, float& y, std::string_view text, Color color)
{
    constexpr float TextGap = 8.0f;
    const UiRect content = uiSubPanelContentRect(panel);
    renderer.drawWrappedText({content.pos.x, y}, text, content.size.x, color, 2);
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

UiRect uiResultDialogOkButtonRect(UiRect panel)
{
    UiRect rect = uiBottomCenterButtonRect(panel, {180.0f, ui::ButtonHeight});
    rect.pos.y += 30.0f;
    return rect;
}

UiRect uiResultDialogTextRect(UiRect panel)
{
    const UiRect ok = uiResultDialogOkButtonRect(panel);
    const float top = panel.pos.y + 72.0f;
    return {{
        panel.pos.x + ui::PanelPadding,
        top,
    }, {
        panel.size.x - ui::PanelPadding * 2.0f,
        std::max(0.0f, ok.pos.y - top - 28.0f),
    }};
}

UiRect uiConfirmDialogMessageRect(UiRect panel)
{
    const UiRect confirm = uiConfirmDialogButtonRect(panel, 0);
    const float top = panel.pos.y + ui::HeaderHeight + 2.0f;
    return {{
        panel.pos.x + 48.0f,
        top,
    }, {
        panel.size.x - 96.0f,
        std::max(0.0f, confirm.pos.y - top - 16.0f),
    }};
}

UiResultDialogLine uiResultDialogPlainLine(std::string text)
{
    UiResultDialogLine line;
    line.segments.push_back({std::move(text), ui::Text});
    return line;
}

std::vector<UiResultDialogLine> uiResultDialogLinesFromText(std::vector<std::string> lines)
{
    std::vector<UiResultDialogLine> result;
    result.reserve(lines.size());
    for (std::string& line : lines) {
        result.push_back(uiResultDialogPlainLine(std::move(line)));
    }
    return result;
}

Vec2 measureUiResultDialogLine(Renderer& renderer, const UiResultDialogLine& line, int scale)
{
    Vec2 size{};
    for (const UiResultDialogSegment& segment : line.segments) {
        const Vec2 segmentSize = renderer.measureText(segment.text, scale);
        size.x += segmentSize.x;
        size.y = std::max(size.y, segmentSize.y);
    }
    if (line.segments.empty()) {
        size.y = renderer.measureText(" ", scale).y;
    }
    return size;
}

void drawCenteredUiResultDialogLine(Renderer& renderer, UiRect rect, float y, const UiResultDialogLine& line, int scale)
{
    const Vec2 size = measureUiResultDialogLine(renderer, line, scale);
    Vec2 pos{rect.pos.x + (rect.size.x - size.x) * 0.5f, y};
    for (const UiResultDialogSegment& segment : line.segments) {
        renderer.drawText(pos, segment.text, segment.color, scale);
        pos.x += renderer.measureText(segment.text, scale).x;
    }
}

UiRect uiQuantityValueRect(UiRect panel)
{
    const UiRect body = uiBodyRect(panel);
    return {{body.pos.x + 28.0f, body.pos.y + 10.0f}, {body.size.x - 56.0f, 132.0f}};
}

UiRect uiQuantityDownButtonRect(UiRect panel)
{
    const UiRect value = uiQuantityValueRect(panel);
    constexpr Vec2 ButtonSize{72.0f, 30.0f};
    return {
        {
            value.pos.x + (value.size.x - ButtonSize.x) * 0.5f,
            value.pos.y + value.size.y - ButtonSize.y - 10.0f,
        },
        ButtonSize,
    };
}

UiRect uiQuantityUpButtonRect(UiRect panel)
{
    const UiRect value = uiQuantityValueRect(panel);
    constexpr Vec2 ButtonSize{72.0f, 30.0f};
    return {
        {value.pos.x + (value.size.x - ButtonSize.x) * 0.5f, value.pos.y + 10.0f},
        ButtonSize,
    };
}

UiRect uiQuantityConfirmButtonRect(UiRect panel)
{
    const UiRect body = uiBodyRect(panel);
    constexpr Vec2 Size{150.0f, ui::ButtonHeight};
    return {{
        body.pos.x + (body.size.x - Size.x) * 0.5f,
        panel.pos.y + panel.size.y - ui::FooterMaxHeight - Size.y - 8.0f,
    }, Size};
}

void closeUiConfirmDialog(UiConfirmDialogState& state)
{
    state.open = false;
    state.title.clear();
    state.message.clear();
    state.confirmLabel = "はい";
    state.cancelLabel = "いいえ";
    state.selection = 1;
    state.confirmEnabled = true;
}

UiButtonStyle uiQuantityStepButtonStyle(bool enabled)
{
    UiButtonStyle style;
    if (enabled) {
        return style;
    }
    style.fill = alphaScaledColor(style.fill, 0.45f);
    style.fillHot = alphaScaledColor(style.fillHot, 0.45f);
    style.outline = alphaScaledColor(style.outline, 0.38f);
    style.outlineHot = alphaScaledColor(style.outlineHot, 0.38f);
    style.text = alphaScaledColor(style.text, 0.42f);
    return style;
}

void closeUiQuantityDialog(UiQuantityDialogState& state)
{
    state.open = false;
    state.title.clear();
    state.message.clear();
    state.unitLabel.clear();
}

void openUiResultDialog(UiResultDialogState& state, std::string title, std::vector<std::string> lines)
{
    openUiResultDialog(state, std::move(title), uiResultDialogLinesFromText(std::move(lines)));
}

void openUiResultDialog(UiResultDialogState& state, std::string title, std::vector<UiResultDialogLine> lines)
{
    state.open = true;
    state.title.clear();
    state.lines = std::move(lines);
    (void)title;
}

UiRect fitUiResultDialogRect(const UiResultDialogState& state, UiRect basePanel)
{
    constexpr int BaseLineCount = 2;
    constexpr float ExtraHeightPerLine = 32.0f;
    const int extraLines = std::max(0, static_cast<int>(state.lines.size()) - BaseLineCount);
    const float extraHeight = static_cast<float>(extraLines) * ExtraHeightPerLine;
    if (extraHeight <= 0.0f) {
        return basePanel;
    }

    basePanel.pos.y -= extraHeight * 0.5f;
    basePanel.size.y += extraHeight;
    return basePanel;
}

bool updateUiResultDialog(UiResultDialogState& state, UiContext& ui, const Input& input, UiRect panel)
{
    if (!state.open) {
        return false;
    }
    panel = fitUiResultDialogRect(state, panel);
    if (ui.pressed(uiResultDialogOkButtonRect(panel)) || input.confirmPressed() || input.useItemPressed()) {
        ui.emitSound(UiSoundEvent::Confirm);
        state.open = false;
        state.title.clear();
        state.lines.clear();
        return true;
    }
    return false;
}

void drawUiResultDialog(Renderer& renderer, const UiResultDialogState& state, UiRect panel, std::string_view id)
{
    if (!state.open) {
        return;
    }
    UiNavigationLayerScope navigationScope;
    panel = fitUiResultDialogRect(state, panel);

    UiWindowScope window(renderer, id, panel, "", "F/Enter OK", UiWindowOptions{true, false});
    const UiRect body = uiResultDialogTextRect(panel);
    float y = body.pos.y;
    constexpr int TextScale = 2;
    for (const UiResultDialogLine& line : state.lines) {
        drawCenteredUiResultDialogLine(renderer, body, y, line, TextScale);
        y += measureUiResultDialogLine(renderer, line, TextScale).y + 10.0f;
    }
    drawUiButton(renderer, uiResultDialogOkButtonRect(panel), "OK", true, uiActionButtonStyle());
}

UiRect uiConfirmDialogButtonRect(UiRect panel, int index)
{
    constexpr Vec2 Size{164.0f, ui::ButtonHeight};
    constexpr float BottomGap = 10.0f;
    constexpr float HorizontalInset = 12.0f;
    const float footerHeight = uiFooterHeight(ConfirmDialogHelpText);
    const float y = panel.pos.y + panel.size.y - footerHeight - ui::ButtonHeight - BottomGap;
    return index == 0
        ? UiRect{{panel.pos.x + panel.size.x - ui::PanelPadding - Size.x - HorizontalInset, y}, Size}
        : UiRect{{panel.pos.x + ui::PanelPadding + HorizontalInset, y}, Size};
}

std::string_view uiConfirmDialogHelpText()
{
    return ConfirmDialogHelpText;
}

void openUiConfirmDialog(
    UiConfirmDialogState& state,
    std::string title,
    std::string message,
    std::string confirmLabel,
    std::string cancelLabel,
    int defaultSelection)
{
    state.open = true;
    state.title = std::move(title);
    state.message = std::move(message);
    state.confirmLabel = std::move(confirmLabel);
    state.cancelLabel = std::move(cancelLabel);
    state.selection = std::clamp(defaultSelection, 0, 1);
    state.confirmEnabled = true;
}

UiConfirmDialogResult updateUiConfirmDialog(UiConfirmDialogState& state, UiContext& ui, const Input& input, UiRect panel)
{
    if (!state.open) {
        return UiConfirmDialogResult::None;
    }

    const int previousSelection = state.selection;
    if (ui.hovered(uiConfirmDialogButtonRect(panel, 0))) {
        state.selection = 0;
    } else if (ui.hovered(uiConfirmDialogButtonRect(panel, 1))) {
        state.selection = 1;
    }
    if (input.pressed(InputAction::MoveLeft) || input.pressed(InputAction::MoveUp) || input.activeRingDelta() < 0) {
        state.selection = 1;
    }
    if (input.pressed(InputAction::MoveRight) || input.pressed(InputAction::MoveDown) || input.activeRingDelta() > 0) {
        state.selection = 0;
    }
    ui.emitCursorMoveIfChanged(previousSelection, state.selection);

    const bool confirmRequested =
        state.confirmEnabled &&
        (ui.pressed(uiConfirmDialogButtonRect(panel, 0)) ||
            ((input.confirmPressed() || input.useItemPressed()) && state.selection == 0));
    const bool backPressed = input.backPressed() && !ui.backInputConsumed();
    const bool cancelRequested =
        ui.pressed(uiConfirmDialogButtonRect(panel, 1)) ||
        ui.pressed(uiCancelButtonRect(panel)) ||
        backPressed ||
        ((input.confirmPressed() || input.useItemPressed()) && state.selection == 1);

    if (confirmRequested) {
        ui.emitSound(UiSoundEvent::Confirm);
        closeUiConfirmDialog(state);
        return UiConfirmDialogResult::Confirmed;
    }
    if (cancelRequested) {
        if (backPressed) {
            ui.consumeBackInput();
        }
        ui.emitSound(UiSoundEvent::Cancel);
        closeUiConfirmDialog(state);
        return UiConfirmDialogResult::Cancelled;
    }
    return UiConfirmDialogResult::None;
}

void drawUiConfirmDialogButtons(Renderer& renderer, const UiConfirmDialogState& state, UiRect panel)
{
    UiButtonStyle confirmStyle = uiActionButtonStyle();
    if (!state.confirmEnabled) {
        confirmStyle.fill = {20, 24, 38, 190};
        confirmStyle.fillHot = confirmStyle.fill;
        confirmStyle.text = ui::TextDisabled;
    }
    drawUiButton(
        renderer,
        uiConfirmDialogButtonRect(panel, 0),
        state.confirmLabel,
        state.selection == 0 && state.confirmEnabled,
        confirmStyle);
    drawUiButton(
        renderer,
        uiConfirmDialogButtonRect(panel, 1),
        state.cancelLabel,
        state.selection == 1,
        uiCancelButtonStyle());
}

void drawUiConfirmDialog(Renderer& renderer, const UiConfirmDialogState& state, UiRect panel, std::string_view id)
{
    if (!state.open) {
        return;
    }

    UiNavigationLayerScope navigationScope;
    UiWindowScope window(renderer, id, panel, state.title, uiConfirmDialogHelpText(), UiWindowOptions{true, true});
    const UiRect message = uiConfirmDialogMessageRect(panel);
    renderer.drawWrappedText(message.pos, state.message, message.size.x, ui::Text, 2);
    drawUiConfirmDialogButtons(renderer, state, panel);
}

void openUiQuantityDialog(
    UiQuantityDialogState& state,
    std::string title,
    std::string message,
    int minValue,
    int maxValue,
    int initialValue,
    std::string unitLabel)
{
    state.open = true;
    state.title = std::move(title);
    state.message = std::move(message);
    state.unitLabel = std::move(unitLabel);
    state.minValue = std::min(minValue, maxValue);
    state.maxValue = std::max(minValue, maxValue);
    state.value = std::clamp(initialValue, state.minValue, state.maxValue);
    state.largeStep = 10;
}

UiQuantityDialogResult updateUiQuantityDialog(UiQuantityDialogState& state, UiContext& ui, const Input& input, UiRect panel)
{
    if (!state.open) {
        return UiQuantityDialogResult::None;
    }

    const int previousValue = state.value;
    const auto adjust = [&state](int delta) {
        if (delta == 0) {
            return;
        }
        state.value = std::clamp(state.value + delta, state.minValue, state.maxValue);
    };

    if (input.pressed(InputAction::MoveUp) || input.pressed(InputAction::MoveRight)) {
        adjust(1);
    }
    if (input.pressed(InputAction::MoveDown) || input.pressed(InputAction::MoveLeft)) {
        adjust(-1);
    }
    if (input.activeRingDelta() != 0) {
        adjust(input.activeRingDelta() * state.largeStep);
    }
    const int directSlot = input.shortcutSlotPressed();
    if (directSlot >= 0) {
        const int directValue = directSlot + 1;
        if (directValue >= state.minValue && directValue <= state.maxValue) {
            state.value = directValue;
        }
    }

    if (state.value > state.minValue && ui.pressed(uiQuantityDownButtonRect(panel))) {
        adjust(-1);
    }
    if (state.value < state.maxValue && ui.pressed(uiQuantityUpButtonRect(panel))) {
        adjust(1);
    }
    ui.emitCursorMoveIfChanged(previousValue, state.value);
    if (ui.pressed(uiQuantityConfirmButtonRect(panel)) ||
        (!ui.navigationActive() && (input.confirmPressed() || input.useItemPressed()))) {
        ui.emitSound(UiSoundEvent::Confirm);
        closeUiQuantityDialog(state);
        return UiQuantityDialogResult::Confirmed;
    }
    const bool backPressed = input.backPressed() && !ui.backInputConsumed();
    if (ui.pressed(uiCancelButtonRect(panel)) || backPressed) {
        if (backPressed) {
            ui.consumeBackInput();
        }
        ui.emitSound(UiSoundEvent::Cancel);
        closeUiQuantityDialog(state);
        return UiQuantityDialogResult::Cancelled;
    }

    return UiQuantityDialogResult::None;
}

void drawUiQuantityDialog(Renderer& renderer, const UiQuantityDialogState& state, UiRect panel, std::string_view id)
{
    if (!state.open) {
        return;
    }

    UiNavigationLayerScope navigationScope;
    UiWindowScope window(renderer, id, panel, state.title, "↑/↓　+1/-1　Z/X　+10/-10　F/Enter　決定", UiWindowOptions{true, true});
    const UiRect body = uiBodyRect(panel);
    const UiRect valueRect = uiQuantityValueRect(panel);
    if (!state.message.empty()) {
        renderer.drawWrappedText({valueRect.pos.x, body.pos.y - 24.0f}, state.message, valueRect.size.x, ui::Text, 2);
    }

    drawUiSubPanel(renderer, valueRect);
    std::string valueText = std::to_string(state.value);
    if (!state.unitLabel.empty()) {
        valueText += state.unitLabel;
    }
    const UiRect upButton = uiQuantityUpButtonRect(panel);
    const UiRect downButton = uiQuantityDownButtonRect(panel);
    const Vec2 valueSize = renderer.measureText(valueText, 4);
    const float textBandTop = upButton.pos.y + upButton.size.y;
    const float textBandBottom = downButton.pos.y;
    renderer.drawText(
        {
            valueRect.pos.x + (valueRect.size.x - valueSize.x) * 0.5f,
            textBandTop + std::max(0.0f, (textBandBottom - textBandTop - valueSize.y) * 0.5f),
        },
        valueText,
        ui::Text,
        4);

    drawUiRectButton(renderer, upButton, "▲", false, uiQuantityStepButtonStyle(state.value < state.maxValue));
    drawUiRectButton(renderer, downButton, "▼", false, uiQuantityStepButtonStyle(state.value > state.minValue));

    drawUiButton(renderer, uiQuantityConfirmButtonRect(panel), "決定", true, uiActionButtonStyle());
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
    state.openSoundPending = true;
}

Vec2 uiCommandMenuAnchorForSlot(UiRect slotRect)
{
    return slotRect.pos + Vec2{slotRect.size.x - 20.0f, 0.0f};
}

void closeUiCommandMenu(UiCommandMenuState& state)
{
    state.open = false;
    state.openSoundPending = false;
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
            state.openSoundPending = false;
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
    if (state.openSoundPending) {
        ui.emitSound(UiSoundEvent::MenuOpen);
        state.openSoundPending = false;
    }

    const bool backPressed = input.backPressed() && !ui.backInputConsumed();
    if (backPressed || input.pressed(InputAction::OffsetRingCenter)) {
        if (backPressed) {
            ui.consumeBackInput();
        }
        ui.emitSound(UiSoundEvent::Cancel);
        closeUiCommandMenu(state);
        return -1;
    }

    if (state.hoveredIndex < 0 || state.hoveredIndex >= itemCount) {
        state.hoveredIndex = 0;
    }

    const int previousHoveredIndex = state.hoveredIndex;
    const int delta = (input.pressed(InputAction::MoveDown) || input.pressed(InputAction::MoveRight) ? 1 : 0) -
        (input.pressed(InputAction::MoveUp) || input.pressed(InputAction::MoveLeft) ? 1 : 0);
    if (delta != 0) {
        state.hoveredIndex = (state.hoveredIndex + delta + itemCount) % itemCount;
    }

    bool hoveredByMouse = false;
    for (int i = 0; i < itemCount; ++i) {
        if (ui.navigationFocused(commandMenuItemRect(state, i))) {
            state.hoveredIndex = i;
        }
        if (ui.hovered(commandMenuItemRect(state, i))) {
            state.hoveredIndex = i;
            hoveredByMouse = true;
            break;
        }
    }
    if (!hoveredByMouse && (state.hoveredIndex < 0 || state.hoveredIndex >= itemCount)) {
        state.hoveredIndex = 0;
    }
    ui.emitCursorMoveIfChanged(previousHoveredIndex, state.hoveredIndex);

    if (input.confirmPressed() || input.useItemPressed()) {
        if (state.hoveredIndex < 0 || state.hoveredIndex >= itemCount || !items[state.hoveredIndex].enabled) {
            return -1;
        }
        const int selected = state.hoveredIndex;
        ui.emitSound(UiSoundEvent::Confirm);
        closeUiCommandMenu(state);
        return selected;
    }

    if (!input.mouseLeftPressed()) {
        return -1;
    }

    const bool insidePanel = state.panel.contains(ui.mouse());
    if (!insidePanel) {
        ui.consumePointer();
        ui.emitSound(UiSoundEvent::Cancel);
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
    ui.emitSound(UiSoundEvent::Confirm);
    closeUiCommandMenu(state);
    return selected;
}

void drawUiCommandMenu(Renderer& renderer, const UiCommandMenuState& state, const UiCommandMenuItem* items, int itemCount)
{
    if (!state.visible || items == nullptr || itemCount <= 0) {
        return;
    }

    UiNavigationLayerScope navigationScope;
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
        registerUiNavigationTarget(rect, UiNavigationRole::Control, hot, items[i].enabled);
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
            requestUiSelectionCursor(rect);
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

namespace {

UiRect scrollAreaTrackRect(const UiScrollAreaLayout& layout, const UiScrollAreaStyle& style)
{
    return {{
        layout.viewport.pos.x + layout.viewport.size.x - style.scrollbarPaddingX - style.scrollbarWidth,
        layout.viewport.pos.y + style.scrollbarPaddingY,
    }, {
        style.scrollbarWidth,
        std::max(1.0f, layout.viewport.size.y - style.scrollbarPaddingY * 2.0f),
    }};
}

UiRect scrollAreaThumbRect(const UiScrollAreaLayout& layout, const UiScrollAreaStyle& style)
{
    const UiRect track = scrollAreaTrackRect(layout, style);
    if (!layout.scrollable || layout.contentHeight <= 0.0f) {
        return track;
    }

    const float visibleRatio = clamp(layout.viewport.size.y / std::max(layout.viewport.size.y, layout.contentHeight), 0.0f, 1.0f);
    const float thumbHeight = std::clamp(
        track.size.y * visibleRatio,
        std::min(track.size.y, style.scrollbarMinThumbHeight),
        track.size.y);
    const float scrollRatio = layout.maxScroll > 0.0f ? layout.scrollOffset / layout.maxScroll : 0.0f;
    return {{
        track.pos.x,
        track.pos.y + (track.size.y - thumbHeight) * scrollRatio,
    }, {track.size.x, thumbHeight}};
}

void scrollAreaSetThumbY(
    const UiScrollAreaLayout& layout,
    const UiScrollAreaStyle& style,
    float thumbY,
    float& scrollOffset)
{
    const UiRect track = scrollAreaTrackRect(layout, style);
    const UiRect thumb = scrollAreaThumbRect(layout, style);
    const float movable = std::max(0.0f, track.size.y - thumb.size.y);
    if (movable <= 0.0f || layout.maxScroll <= 0.0f) {
        scrollOffset = 0.0f;
        return;
    }

    const float ratio = clamp((thumbY - track.pos.y) / movable, 0.0f, 1.0f);
    scrollOffset = ratio * layout.maxScroll;
}

}

UiScrollAreaLayout makeUiScrollAreaLayout(UiRect viewport, float contentHeight, float scrollOffset, const UiScrollAreaStyle& style)
{
    UiScrollAreaLayout layout;
    layout.viewport = viewport;
    layout.contentHeight = std::max(0.0f, contentHeight);
    layout.maxScroll = std::max(0.0f, layout.contentHeight - std::max(0.0f, viewport.size.y));
    layout.scrollable = layout.maxScroll > 0.0f;
    layout.scrollOffset = clamp(scrollOffset, 0.0f, layout.maxScroll);
    layout.scrollbarReserve = layout.scrollable
        ? std::max(0.0f, style.scrollbarWidth + style.scrollbarGap + style.scrollbarPaddingX)
        : 0.0f;
    layout.content = viewport;
    layout.content.size.x = std::max(0.0f, layout.content.size.x - layout.scrollbarReserve);
    return layout;
}

UiScrollAreaLayout updateUiScrollArea(
    UiContext& ui,
    const Input& input,
    UiRect viewport,
    float contentHeight,
    float& scrollOffset,
    const UiScrollAreaStyle& style,
    UiScrollAreaState* state)
{
    UiScrollAreaLayout layout = makeUiScrollAreaLayout(viewport, contentHeight, scrollOffset, style);
    if (state != nullptr && state->draggingScrollbar) {
        if (input.mouseLeftHeld() && layout.scrollable) {
            scrollAreaSetThumbY(layout, style, ui.mouse().y - state->scrollbarDragOffsetY, scrollOffset);
            layout = makeUiScrollAreaLayout(viewport, contentHeight, scrollOffset, style);
            ui.consumePointer();
            return layout;
        }
        state->draggingScrollbar = false;
    }

    if (state != nullptr && layout.scrollable && input.mouseLeftPressed() && !ui.pointerConsumed()) {
        const UiRect track = scrollAreaTrackRect(layout, style);
        if (track.contains(ui.mouse())) {
            const UiRect thumb = scrollAreaThumbRect(layout, style);
            state->draggingScrollbar = true;
            state->scrollbarDragOffsetY = thumb.contains(ui.mouse())
                ? ui.mouse().y - thumb.pos.y
                : thumb.size.y * 0.5f;
            scrollAreaSetThumbY(layout, style, ui.mouse().y - state->scrollbarDragOffsetY, scrollOffset);
            layout = makeUiScrollAreaLayout(viewport, contentHeight, scrollOffset, style);
            ui.consumePointer();
            return layout;
        }
    }

    const int wheel = input.mouseWheelDelta();
    if (wheel != 0 && layout.scrollable && viewport.contains(ui.mouse())) {
        scrollOffset = clamp(
            layout.scrollOffset + static_cast<float>(wheel) * std::max(1.0f, style.wheelStep),
            0.0f,
            layout.maxScroll);
        layout = makeUiScrollAreaLayout(viewport, contentHeight, scrollOffset, style);
    } else {
        scrollOffset = layout.scrollOffset;
    }
    return layout;
}

bool uiScrollAreaRectVisible(const UiScrollAreaLayout& layout, UiRect rect)
{
    return rect.pos.y + rect.size.y >= layout.viewport.pos.y &&
        rect.pos.y <= layout.viewport.pos.y + layout.viewport.size.y &&
        rect.pos.x + rect.size.x >= layout.viewport.pos.x &&
        rect.pos.x <= layout.viewport.pos.x + layout.content.size.x;
}

void keepUiScrollAreaRectVisible(UiRect viewport, UiRect rect, float contentHeight, float& scrollOffset, const UiScrollAreaStyle& style)
{
    const UiScrollAreaLayout layout = makeUiScrollAreaLayout(viewport, contentHeight, scrollOffset, style);
    const float top = viewport.pos.y;
    const float bottom = viewport.pos.y + viewport.size.y;
    if (rect.pos.y < top) {
        scrollOffset -= top - rect.pos.y;
    } else if (rect.pos.y + rect.size.y > bottom) {
        scrollOffset += rect.pos.y + rect.size.y - bottom;
    }
    scrollOffset = clamp(scrollOffset, 0.0f, layout.maxScroll);
}

void drawUiScrollAreaFrame(Renderer& renderer, const UiScrollAreaLayout& layout, const UiScrollAreaStyle& style)
{
    renderer.drawRect(layout.viewport.pos, layout.viewport.size, style.outline);
    drawUiScrollAreaScrollbar(renderer, layout, style);
}

void drawUiScrollAreaScrollbar(Renderer& renderer, const UiScrollAreaLayout& layout, const UiScrollAreaStyle& style)
{
    if (!layout.scrollable || layout.contentHeight <= 0.0f || layout.viewport.size.y <= 0.0f) {
        return;
    }

    const UiRect track = scrollAreaTrackRect(layout, style);
    const UiRect thumb = scrollAreaThumbRect(layout, style);
    renderer.fillRect(track.pos, track.size, style.scrollbarTrack);
    renderer.fillRect(thumb.pos, thumb.size, style.scrollbarThumb);
}

float uiScrollableListContentHeight(int itemCount, const UiScrollableListStyle& style)
{
    if (itemCount <= 0) {
        return style.leadingPadding + style.trailingPadding;
    }
    return style.leadingPadding +
        static_cast<float>(itemCount) * style.rowHeight +
        static_cast<float>(itemCount - 1) * style.rowGap +
        style.trailingPadding;
}

UiScrollAreaLayout makeUiScrollableListLayout(UiRect viewport, int itemCount, float scrollOffset, const UiScrollableListStyle& style)
{
    return makeUiScrollAreaLayout(viewport, uiScrollableListContentHeight(itemCount, style), scrollOffset, style.scroll);
}

UiScrollAreaLayout updateUiScrollableList(
    UiContext& ui,
    const Input& input,
    UiRect viewport,
    int itemCount,
    float& scrollOffset,
    const UiScrollableListStyle& style,
    UiScrollAreaState* state)
{
    return updateUiScrollArea(
        ui,
        input,
        viewport,
        uiScrollableListContentHeight(itemCount, style),
        scrollOffset,
        style.scroll,
        state);
}

UiRect uiScrollableListItemRect(const UiScrollAreaLayout& layout, int index, const UiScrollableListStyle& style)
{
    return {
        {
            layout.content.pos.x + style.rowInsetX,
            layout.content.pos.y + style.leadingPadding + static_cast<float>(std::max(0, index)) * (style.rowHeight + style.rowGap) - layout.scrollOffset,
        },
        {
            std::max(0.0f, layout.content.size.x - style.rowInsetX * 2.0f),
            style.rowHeight,
        },
    };
}

void keepUiScrollableListItemVisible(UiRect viewport, int selectedIndex, int itemCount, float& scrollOffset, const UiScrollableListStyle& style)
{
    if (selectedIndex < 0 || selectedIndex >= itemCount) {
        scrollOffset = makeUiScrollableListLayout(viewport, itemCount, scrollOffset, style).scrollOffset;
        return;
    }
    const UiScrollAreaLayout layout = makeUiScrollableListLayout(viewport, itemCount, scrollOffset, style);
    keepUiScrollAreaRectVisible(
        viewport,
        uiScrollableListItemRect(layout, selectedIndex, style),
        uiScrollableListContentHeight(itemCount, style),
        scrollOffset,
        style.scroll);
}

namespace {

float selectableTableColumnWidth(
    const UiScrollAreaLayout& scroll,
    const UiSelectableTableColumn* columns,
    int columnCount,
    int column,
    const UiSelectableTableStyle& style)
{
    if (columns == nullptr || columnCount <= 0 || column < 0 || column >= columnCount) {
        return 0.0f;
    }

    float fixedWidth = 0.0f;
    int flexibleCount = 0;
    for (int i = 0; i < columnCount; ++i) {
        if (columns[i].width > 0.0f) {
            fixedWidth += columns[i].width;
        } else {
            ++flexibleCount;
        }
    }

    const float totalGap = std::max(0, columnCount - 1) * std::max(0.0f, style.columnGap);
    const float available = std::max(0.0f, scroll.content.size.x - totalGap);
    if (columns[column].width > 0.0f) {
        return std::min(columns[column].width, available);
    }
    if (flexibleCount <= 0) {
        return 0.0f;
    }
    return std::max(1.0f, (available - fixedWidth) / static_cast<float>(flexibleCount));
}

float selectableTableColumnX(
    const UiScrollAreaLayout& scroll,
    const UiSelectableTableColumn* columns,
    int columnCount,
    int column,
    const UiSelectableTableStyle& style)
{
    float x = scroll.content.pos.x;
    for (int i = 0; i < column; ++i) {
        x += selectableTableColumnWidth(scroll, columns, columnCount, i, style) + std::max(0.0f, style.columnGap);
    }
    return x;
}

void clampSelectableTableState(UiSelectableTableState& state, int rowCount, const UiSelectableTableColumn* columns, int columnCount)
{
    if (rowCount <= 0 || columnCount <= 0 || columns == nullptr) {
        state.selectedRow = 0;
        state.selectedColumn = 0;
        return;
    }

    state.selectedRow = std::clamp(state.selectedRow, 0, rowCount - 1);
    state.selectedColumn = std::clamp(state.selectedColumn, 0, columnCount - 1);
    if (!columns[state.selectedColumn].enabled) {
        for (int offset = 1; offset < columnCount; ++offset) {
            const int right = state.selectedColumn + offset;
            if (right < columnCount && columns[right].enabled) {
                state.selectedColumn = right;
                return;
            }
            const int left = state.selectedColumn - offset;
            if (left >= 0 && columns[left].enabled) {
                state.selectedColumn = left;
                return;
            }
        }
    }
}

bool moveSelectableTableColumn(UiSelectableTableState& state, int delta, const UiSelectableTableColumn* columns, int columnCount)
{
    if (delta == 0 || columns == nullptr || columnCount <= 0) {
        return false;
    }

    const int start = state.selectedColumn;
    int next = start;
    do {
        next += delta;
        if (next < 0 || next >= columnCount) {
            return false;
        }
    } while (!columns[next].enabled);

    state.selectedColumn = next;
    return state.selectedColumn != start;
}

}

float uiSelectableTableContentHeight(int rowCount, const UiSelectableTableStyle& style)
{
    if (rowCount <= 0) {
        return 0.0f;
    }
    return static_cast<float>(rowCount) * style.rowHeight +
        static_cast<float>(rowCount - 1) * style.rowGap;
}

UiSelectableTableLayout makeUiSelectableTableLayout(
    UiRect rect,
    int rowCount,
    float scrollOffset,
    const UiSelectableTableStyle& style)
{
    UiSelectableTableLayout layout;
    layout.header = {rect.pos, {rect.size.x, std::max(0.0f, style.headerHeight)}};
    const UiRect viewport{
        {rect.pos.x, rect.pos.y + layout.header.size.y + std::max(0.0f, style.rowGap)},
        {rect.size.x, std::max(0.0f, rect.size.y - layout.header.size.y - std::max(0.0f, style.rowGap))},
    };
    layout.scroll = makeUiScrollAreaLayout(viewport, uiSelectableTableContentHeight(rowCount, style), scrollOffset, style.scroll);
    return layout;
}

UiSelectableTableResult updateUiSelectableTable(
    UiSelectableTableState& state,
    UiContext& ui,
    const Input& input,
    UiRect rect,
    int rowCount,
    const UiSelectableTableColumn* columns,
    int columnCount,
    const UiSelectableTableStyle& style)
{
    UiSelectableTableResult result;
    clampSelectableTableState(state, rowCount, columns, columnCount);
    UiSelectableTableLayout layout = makeUiSelectableTableLayout(rect, rowCount, state.scrollOffset, style);
    layout.scroll = updateUiScrollArea(
        ui,
        input,
        layout.scroll.viewport,
        uiSelectableTableContentHeight(rowCount, style),
        state.scrollOffset,
        style.scroll,
        &state.scroll);
    layout = makeUiSelectableTableLayout(rect, rowCount, state.scrollOffset, style);

    if (rowCount <= 0 || columnCount <= 0 || columns == nullptr) {
        return result;
    }

    const int previousRow = state.selectedRow;
    const int previousColumn = state.selectedColumn;
    if (input.pressed(InputAction::MoveUp)) {
        state.selectedRow = std::max(0, state.selectedRow - 1);
    }
    if (input.pressed(InputAction::MoveDown)) {
        state.selectedRow = std::min(rowCount - 1, state.selectedRow + 1);
    }
    if (input.pressed(InputAction::MoveLeft)) {
        moveSelectableTableColumn(state, -1, columns, columnCount);
    }
    if (input.pressed(InputAction::MoveRight)) {
        moveSelectableTableColumn(state, 1, columns, columnCount);
    }

    for (int row = 0; row < rowCount; ++row) {
        const UiRect rowRect = uiSelectableTableRowRect(layout, row, style);
        if (!uiScrollAreaRectVisible(layout.scroll, rowRect)) {
            continue;
        }
        for (int column = 0; column < columnCount; ++column) {
            if (!columns[column].enabled) {
                continue;
            }
            const UiRect cellRect = uiSelectableTableCellRect(layout, columns, columnCount, row, column, style);
            if (ui.hovered(cellRect)) {
                state.selectedRow = row;
                state.selectedColumn = column;
            }
            if (ui.pressed(cellRect)) {
                state.selectedRow = row;
                state.selectedColumn = column;
                result.pressedRow = row;
                result.pressedColumn = column;
            }
        }
    }

    result.selectionChanged = state.selectedRow != previousRow || state.selectedColumn != previousColumn;
    ui.emitCursorMoveIfChanged(
        previousRow * columnCount + previousColumn,
        state.selectedRow * columnCount + state.selectedColumn);
    keepUiSelectableTableCellVisible(rect, state.selectedRow, rowCount, state.scrollOffset, style);
    return result;
}

UiRect uiSelectableTableRowRect(const UiSelectableTableLayout& layout, int row, const UiSelectableTableStyle& style)
{
    return {
        {
            layout.scroll.content.pos.x,
            layout.scroll.content.pos.y + static_cast<float>(std::max(0, row)) * (style.rowHeight + style.rowGap) - layout.scroll.scrollOffset,
        },
        {layout.scroll.content.size.x, style.rowHeight},
    };
}

UiRect uiSelectableTableCellRect(
    const UiSelectableTableLayout& layout,
    const UiSelectableTableColumn* columns,
    int columnCount,
    int row,
    int column,
    const UiSelectableTableStyle& style)
{
    const UiRect rowRect = uiSelectableTableRowRect(layout, row, style);
    return {
        {selectableTableColumnX(layout.scroll, columns, columnCount, column, style), rowRect.pos.y},
        {selectableTableColumnWidth(layout.scroll, columns, columnCount, column, style), rowRect.size.y},
    };
}

void keepUiSelectableTableCellVisible(
    UiRect rect,
    int row,
    int rowCount,
    float& scrollOffset,
    const UiSelectableTableStyle& style)
{
    const UiSelectableTableLayout layout = makeUiSelectableTableLayout(rect, rowCount, scrollOffset, style);
    if (row < 0 || row >= rowCount) {
        scrollOffset = layout.scroll.scrollOffset;
        return;
    }
    keepUiScrollAreaRectVisible(
        layout.scroll.viewport,
        uiSelectableTableRowRect(layout, row, style),
        uiSelectableTableContentHeight(rowCount, style),
        scrollOffset,
        style.scroll);
}

void drawUiSelectableTableFrame(
    Renderer& renderer,
    const UiSelectableTableLayout& layout,
    const UiSelectableTableColumn* columns,
    int columnCount,
    const UiSelectableTableStyle& style)
{
    if (columns == nullptr || columnCount <= 0) {
        drawUiScrollAreaFrame(renderer, layout.scroll, style.scroll);
        return;
    }

    renderer.fillRect(layout.header.pos, layout.header.size, style.headerFill);
    for (int column = 0; column < columnCount; ++column) {
        const UiRect cell{
            {
                selectableTableColumnX(layout.scroll, columns, columnCount, column, style),
                layout.header.pos.y,
            },
            {
                selectableTableColumnWidth(layout.scroll, columns, columnCount, column, style),
                layout.header.size.y,
            },
        };
        renderer.drawRect(cell.pos, cell.size, style.cellOutline);
        const Vec2 textSize = renderer.measureText(columns[column].label, style.headerTextScale);
        const Vec2 textPos{
            cell.pos.x + std::max(0.0f, (cell.size.x - textSize.x) * 0.5f),
            cell.pos.y + std::max(0.0f, (cell.size.y - textSize.y) * 0.5f),
        };
        renderer.drawText(textPos, columns[column].label, style.headerText, style.headerTextScale);
    }
    drawUiScrollAreaFrame(renderer, layout.scroll, style.scroll);
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
            ui.emitSound(UiSoundEvent::MenuOpen);
        } else {
            ui.emitSound(UiSoundEvent::Cancel);
        }
        return -1;
    }

    if (!state.open) {
        return -1;
    }

    clampDropdownState(state, count, selectedIndex, style);
    const UiRect listRect = uiDropdownListRect(buttonRect, count, style);

    if (count > 0) {
        const int previousHighlightedIndex = state.highlightedIndex;
        int move = 0;
        if (input.pressed(InputAction::MoveDown) || input.pressed(InputAction::MoveRight)) {
            ++move;
        }
        if (input.pressed(InputAction::MoveUp) || input.pressed(InputAction::MoveLeft)) {
            --move;
        }
        moveDropdownHighlight(state, move, count, style);

        const int wheel = input.mouseWheelDelta();
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
            if (ui.selectionFocused(itemHotRect)) {
                state.highlightedIndex = itemIndex;
            }
            if (ui.pressed(itemHotRect)) {
                if (items != nullptr && items[itemIndex].enabled) {
                    selected = itemIndex;
                    state.open = false;
                    ui.emitSound(UiSoundEvent::Confirm);
                }
                return selected;
            }
        }
        ui.emitCursorMoveIfChanged(previousHighlightedIndex, state.highlightedIndex);

        if (input.confirmPressed() || input.useItemPressed()) {
            const int index = state.highlightedIndex;
            if (items != nullptr && index >= 0 && index < count && items[index].enabled) {
                selected = index;
                state.open = false;
                ui.emitSound(UiSoundEvent::Confirm);
                return selected;
            }
        }
    }

    if (input.backPressed() && !ui.backInputConsumed()) {
        ui.consumeBackInput();
        state.open = false;
        ui.emitSound(UiSoundEvent::Cancel);
        return -1;
    }

    if (input.mouseLeftPressed() && !ui.pointerConsumed() && !buttonRect.contains(ui.mouse()) && !listRect.contains(ui.mouse())) {
        ui.consumePointer();
        state.open = false;
        ui.emitSound(UiSoundEvent::Cancel);
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

    UiNavigationLayerScope navigationScope;
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
        registerUiNavigationTarget(
            rowBody,
            UiNavigationRole::Control,
            highlighted,
            items[itemIndex].enabled);
        if (highlighted) {
            renderer.fillRect(rowBody.pos, rowBody.size, style.fillHot);
            requestUiSelectionCursor(rowBody);
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
    state.hoveredIndex = -1;

    for (int i = 0; i < itemCount; ++i) {
        if (tabItemEnabled(items, i) && ui.navigationFocused(rects[i])) {
            state.focusedIndex = i;
        }
        if (tabItemEnabled(items, i) && ui.hovered(rects[i])) {
            state.focusedIndex = i;
            state.hoveredIndex = i;
        }
        if (ui.pressed(rects[i])) {
            if (!tabItemEnabled(items, i)) {
                return -1;
            }
            state.focusedIndex = i;
            if (i != selectedIndex) {
                ui.emitSound(UiSoundEvent::TabSwitch);
            }
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
        ui.emitSound(UiSoundEvent::TabSwitch);
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

    constexpr float TabTextOffsetY = 2.0f;
    const int preferredIndex = state.focusedIndex >= 0 ? state.focusedIndex : selectedIndex;
    for (int i = 0; i < itemCount; ++i) {
        UiRect navigationRect = rects[i];
        navigationRect.size.y = ui::ButtonHeight;
        registerUiNavigationTarget(
            navigationRect,
            UiNavigationRole::Tab,
            i == preferredIndex,
            tabItemEnabled(items, i));
    }

    if (renderer.hasUiHorizontalTabTexture()) {
        std::vector<Vec2> imagePositions;
        std::vector<Vec2> imageSizes;
        std::vector<int> selectedFlags;
        std::vector<Color> imageTints;
        imagePositions.reserve(static_cast<std::size_t>(itemCount));
        imageSizes.reserve(static_cast<std::size_t>(itemCount));
        selectedFlags.reserve(static_cast<std::size_t>(itemCount));
        imageTints.reserve(static_cast<std::size_t>(itemCount));

        const float imageOutset = std::max(0.0f, style.imageOutset);
        for (int i = 0; i < itemCount; ++i) {
            UiButtonStyle buttonStyle = style.buttonStyle;
            const bool selected = i == selectedIndex;
            if (selected) {
                buttonStyle.text = style.selectedText;
                buttonStyle.imageTintHot = style.selectedImageTint;
            }

            UiRect rect = rects[i];
            rect.size.y = ui::ButtonHeight;
            const UiRect imageRect{
                rect.pos - Vec2{imageOutset, imageOutset},
                rect.size + Vec2{imageOutset * 2.0f, imageOutset * 2.0f},
            };

            Color tint = selected ? buttonStyle.imageTintHot : buttonStyle.imageTint;
            const bool enabled = tabItemEnabled(items, i);
            if (i == state.focusedIndex && enabled) {
                tint = scaledColor(tint, 1.08f);
            }
            if (!enabled) {
                tint = alphaScaledColor(tint, 0.55f);
            }

            imagePositions.push_back(imageRect.pos);
            imageSizes.push_back(imageRect.size);
            selectedFlags.push_back(selected ? 1 : 0);
            imageTints.push_back(tint);
        }

        int runStart = 0;
        for (int i = 1; i <= itemCount; ++i) {
            const bool breakRun = i == itemCount ||
                std::abs(imagePositions[i].y - imagePositions[i - 1].y) > 1.0f ||
                imagePositions[i].x < imagePositions[i - 1].x;
            if (!breakRun) {
                continue;
            }
            renderer.drawUiHorizontalTabs(
                imagePositions.data() + runStart,
                imageSizes.data() + runStart,
                selectedFlags.data() + runStart,
                imageTints.data() + runStart,
                i - runStart);
            runStart = i;
        }

        for (int i = 0; i < itemCount; ++i) {
            UiButtonStyle buttonStyle = style.buttonStyle;
            const bool selected = i == selectedIndex;
            const bool enabled = tabItemEnabled(items, i);
            if (selected) {
                buttonStyle.text = style.selectedText;
            }
            if (!enabled) {
                buttonStyle.text = ui::TextDisabled;
            }

            UiRect rect = rects[i];
            rect.size.y = ui::ButtonHeight;
            Color iconTint = selected ? style.selectedImageTint : Color{255, 255, 255, 255};
            if (!enabled) {
                iconTint = alphaScaledColor(iconTint, 0.45f);
            }
            drawUiLabelWithOptionalIcon(
                renderer,
                rect,
                items[i].label,
                items[i].iconImageNumber,
                buttonStyle.text,
                2,
                UiTabIconSize,
                TabTextOffsetY,
                iconTint);
            if (i == state.focusedIndex && enabled) {
                requestUiSelectionCursor(rect);
            }
        }
        return;
    }

    auto drawTab = [&](int i) {
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
        UiRect rect = rects[i];
        rect.size.y = ui::ButtonHeight;
        const float visualInsetX = std::min(std::max(0.0f, style.visualGap * 0.5f), std::max(0.0f, rect.size.x * 0.25f));
        rect.pos.x += visualInsetX;
        rect.size.x = std::max(1.0f, rect.size.x - visualInsetX * 2.0f);

        const bool selected = i == selectedIndex;
        const bool active = selected || (i == state.focusedIndex && tabItemEnabled(items, i));
        const float scale = active ? std::max(1.0f, style.activeScale) : 1.0f;
        const Vec2 center = rect.pos + rect.size * 0.5f;
        renderer.pushScreenTransform(center, scale, 1.0f);

        const float imageOutset = std::max(0.0f, style.imageOutset);
        const UiRect imageRect{
            rect.pos - Vec2{imageOutset, imageOutset},
            rect.size + Vec2{imageOutset * 2.0f, imageOutset * 2.0f},
        };

        if (renderer.hasUiTabTexture()) {
            const Color tint = selected ? buttonStyle.imageTintHot : buttonStyle.imageTint;
            renderer.drawUiTabFrame(imageRect.pos, imageRect.size, selected, tint);

            Color iconTint = selected ? style.selectedImageTint : Color{255, 255, 255, 255};
            if (!tabItemEnabled(items, i)) {
                iconTint = alphaScaledColor(iconTint, 0.45f);
            }
            drawUiLabelWithOptionalIcon(
                renderer,
                rect,
                items[i].label,
                items[i].iconImageNumber,
                buttonStyle.text,
                2,
                UiTabIconSize,
                TabTextOffsetY,
                iconTint);
        } else {
            const Color fill = selected ? buttonStyle.fillHot : buttonStyle.fill;
            const Color outline = selected ? scaledColor(buttonStyle.outlineHot, 1.04f) : buttonStyle.outline;
            renderer.fillRect(imageRect.pos, imageRect.size, fill);
            renderer.drawRect(imageRect.pos, imageRect.size, outline);

            Color iconTint = selected ? style.selectedImageTint : Color{255, 255, 255, 255};
            if (!tabItemEnabled(items, i)) {
                iconTint = alphaScaledColor(iconTint, 0.45f);
            }
            drawUiLabelWithOptionalIcon(
                renderer,
                rect,
                items[i].label,
                items[i].iconImageNumber,
                buttonStyle.text,
                2,
                UiTabIconSize,
                TabTextOffsetY,
                iconTint);
        }
        renderer.popScreenTransform();
        if (active) {
            requestUiSelectionCursor(rect);
        }
    };

    for (int i = 0; i < itemCount; ++i) {
        const bool active = i == selectedIndex || (i == state.focusedIndex && tabItemEnabled(items, i));
        if (!active) {
            drawTab(i);
        }
    }
    for (int i = 0; i < itemCount; ++i) {
        const bool active = i == selectedIndex || (i == state.focusedIndex && tabItemEnabled(items, i));
        if (active) {
            drawTab(i);
        }
    }
}

int updateUiSubTabs(
    UiTabsState& state,
    UiContext& ui,
    const UiTabsInput& input,
    int selectedIndex,
    const UiTabItem* items,
    int itemCount,
    const UiRect* rects,
    const UiSubTabsStyle& style)
{
    UiTabsStyle inputStyle;
    inputStyle.wrapKeyboard = style.wrapKeyboard;
    return updateUiTabs(state, ui, input, selectedIndex, items, itemCount, rects, inputStyle);
}

void drawUiSubTabs(
    Renderer& renderer,
    const UiTabsState& state,
    int selectedIndex,
    const UiTabItem* items,
    int itemCount,
    const UiRect* rects,
    const UiSubTabsStyle& style)
{
    if (items == nullptr || rects == nullptr || itemCount <= 0) {
        return;
    }
    const int preferredIndex = state.focusedIndex >= 0 ? state.focusedIndex : selectedIndex;
    for (int i = 0; i < itemCount; ++i) {
        registerUiNavigationTarget(
            rects[i],
            UiNavigationRole::Tab,
            i == preferredIndex,
            tabItemEnabled(items, i));
    }

    UiRect bounds = rects[0];
    for (int i = 1; i < itemCount; ++i) {
        const float left = std::min(bounds.pos.x, rects[i].pos.x);
        const float top = std::min(bounds.pos.y, rects[i].pos.y);
        const float right = std::max(bounds.pos.x + bounds.size.x, rects[i].pos.x + rects[i].size.x);
        const float bottom = std::max(bounds.pos.y + bounds.size.y, rects[i].pos.y + rects[i].size.y);
        bounds = {{left, top}, {right - left, bottom - top}};
    }

    const float fadeWidth = std::max(0.0f, style.fadeWidth);
    const float sidePadding = std::max(fadeWidth, style.sidePadding);
    const UiRect bar{
        {bounds.pos.x - sidePadding, bounds.pos.y},
        {bounds.size.x + sidePadding * 2.0f, bounds.size.y},
    };

    const float leftFadeWidth = std::min(fadeWidth, std::max(0.0f, bar.size.x * 0.5f));
    const float rightFadeWidth = leftFadeWidth;
    const float centerWidth = std::max(0.0f, bar.size.x - leftFadeWidth - rightFadeWidth);
    const Color transparent{style.barFill.r, style.barFill.g, style.barFill.b, 0};
    if (leftFadeWidth > 0.0f) {
        renderer.fillGradientRect(bar.pos, {leftFadeWidth, bar.size.y}, transparent, style.barFill, GradientDirection::LeftToRight);
    }
    if (centerWidth > 0.0f) {
        renderer.fillRect({bar.pos.x + leftFadeWidth, bar.pos.y}, {centerWidth, bar.size.y}, style.barFill);
    }
    if (rightFadeWidth > 0.0f) {
        renderer.fillGradientRect(
            {bar.pos.x + bar.size.x - rightFadeWidth, bar.pos.y},
            {rightFadeWidth, bar.size.y},
            style.barFill,
            transparent,
            GradientDirection::LeftToRight);
    }

    const auto fillFadedTab = [&](UiRect rect, Color color) {
        const float tabFadeWidth = std::min(
            std::max(0.0f, style.tabFadeWidth),
            std::max(0.0f, rect.size.x * 0.5f));
        const float tabCenterWidth = std::max(0.0f, rect.size.x - tabFadeWidth * 2.0f);
        const Color tabTransparent{color.r, color.g, color.b, 0};
        if (tabFadeWidth > 0.0f) {
            renderer.fillGradientRect(rect.pos, {tabFadeWidth, rect.size.y}, tabTransparent, color, GradientDirection::LeftToRight);
        }
        if (tabCenterWidth > 0.0f) {
            renderer.fillRect({rect.pos.x + tabFadeWidth, rect.pos.y}, {tabCenterWidth, rect.size.y}, color);
        }
        if (tabFadeWidth > 0.0f) {
            renderer.fillGradientRect(
                {rect.pos.x + rect.size.x - tabFadeWidth, rect.pos.y},
                {tabFadeWidth, rect.size.y},
                color,
                tabTransparent,
                GradientDirection::LeftToRight);
        }
    };

    const int textScale = std::max(1, style.textScale);
    for (int i = 0; i < itemCount; ++i) {
        const bool enabled = tabItemEnabled(items, i);
        const bool selected = i == selectedIndex;
        const bool hovered = enabled && i == state.hoveredIndex;
        const UiRect rect = rects[i];

        if (selected) {
            fillFadedTab(rect, hovered ? scaledColor(style.selectedFill, 1.08f) : style.selectedFill);
        } else if (hovered) {
            fillFadedTab(rect, style.hoverFill);
        }

        const Color textColor = enabled
            ? (selected ? style.selectedText : style.text)
            : style.disabledText;
        Color iconTint = selected ? style.selectedText : Color{255, 255, 255, 255};
        if (!enabled) {
            iconTint = alphaScaledColor(iconTint, 0.45f);
        }
        drawUiLabelWithOptionalIcon(
            renderer,
            rect,
            items[i].label,
            items[i].iconImageNumber,
            textColor,
            textScale,
            UiTabIconSize,
            style.textOffsetY,
            iconTint);
        if (enabled && (i == state.focusedIndex || selected)) {
            requestUiSelectionCursor(rect);
        }
    }
}

int updateUiVerticalTabs(
    UiTabsState& state,
    UiContext& ui,
    const UiTabsInput& input,
    int selectedIndex,
    const UiVerticalTabItem* items,
    int itemCount,
    const UiRect* rects,
    const UiVerticalTabsStyle& style)
{
    if (items == nullptr || rects == nullptr || itemCount <= 0) {
        state.focusedIndex = -1;
        return -1;
    }

    std::vector<UiTabItem> tabItems;
    tabItems.reserve(static_cast<std::size_t>(itemCount));
    for (int i = 0; i < itemCount; ++i) {
        tabItems.push_back({items[i].label, items[i].enabled});
    }
    return updateUiTabs(state, ui, input, selectedIndex, tabItems.data(), itemCount, rects, style.tabs);
}

void drawUiVerticalTabs(
    Renderer& renderer,
    const UiTabsState& state,
    int selectedIndex,
    const UiVerticalTabItem* items,
    int itemCount,
    const UiRect* rects,
    const UiVerticalTabsStyle& style)
{
    if (items == nullptr || rects == nullptr || itemCount <= 0) {
        return;
    }
    const int preferredIndex = state.focusedIndex >= 0 ? state.focusedIndex : selectedIndex;
    for (int i = 0; i < itemCount; ++i) {
        registerUiNavigationTarget(
            rects[i],
            UiNavigationRole::Control,
            i == preferredIndex,
            items[i].enabled);
    }

    auto itemEnabled = [&](int index) {
        return index >= 0 && index < itemCount && items[index].enabled;
    };

    auto drawTab = [&](int i) {
        const bool enabled = itemEnabled(i);
        const bool selected = i == selectedIndex;
        const bool active = selected || (i == state.focusedIndex && enabled);
        const float scale = active ? std::max(1.0f, style.tabs.activeScale) : 1.0f;
        const UiRect rect = rects[i];
        const Vec2 center = rect.pos + rect.size * 0.5f;
        renderer.pushScreenTransform(center, scale, 1.0f);

        const float imageOutset = std::max(0.0f, style.tabs.imageOutset);
        const UiRect imageRect{
            rect.pos - Vec2{imageOutset, imageOutset},
            rect.size + Vec2{imageOutset * 2.0f, imageOutset * 2.0f},
        };

        UiButtonStyle buttonStyle = style.tabs.buttonStyle;
        if (selected) {
            buttonStyle.fillHot = style.tabs.selectedFillHot;
            buttonStyle.outlineHot = style.tabs.selectedOutlineHot;
            buttonStyle.text = style.tabs.selectedText;
            buttonStyle.imageTintHot = style.tabs.selectedImageTint;
        }

        if (renderer.hasUiTabTexture()) {
            Color tint = selected ? buttonStyle.imageTintHot : buttonStyle.imageTint;
            if (!enabled) {
                tint = alphaScaledColor(tint, 0.55f);
            }
            renderer.drawUiTabFrame(imageRect.pos, imageRect.size, selected, tint);
        } else {
            Color fill = selected ? buttonStyle.fillHot : buttonStyle.fill;
            Color outline = selected ? scaledColor(buttonStyle.outlineHot, 1.04f) : buttonStyle.outline;
            if (!enabled) {
                fill = alphaScaledColor(fill, 0.55f);
                outline = alphaScaledColor(outline, 0.55f);
            }
            renderer.fillRect(imageRect.pos, imageRect.size, fill);
            renderer.drawRect(imageRect.pos, imageRect.size, outline);
        }

        const int labelScale = std::max(1, style.labelScale);
        const int valueScale = std::max(1, style.valueScale);
        const bool hasValue = !items[i].value.empty();
        const Vec2 valueSize = hasValue ? renderer.measureText(items[i].value, valueScale) : Vec2{};
        const float labelX = rect.pos.x + std::max(0.0f, style.textPaddingX);
        const float valueX = rect.pos.x + rect.size.x - valueSize.x - std::max(0.0f, style.valuePaddingX);
        const float labelRight = hasValue
            ? valueX - std::max(0.0f, style.valueGap)
            : rect.pos.x + rect.size.x - std::max(0.0f, style.textPaddingX);
        const std::string label = fittedUiText(renderer, items[i].label, std::max(0.0f, labelRight - labelX), labelScale);
        const Vec2 labelSize = renderer.measureText(label, labelScale);
        const float lineHeight = std::max(labelSize.y, valueSize.y);
        const float labelY = rect.pos.y + std::max(0.0f, (rect.size.y - lineHeight) * 0.5f) + style.textOffsetY;
        const float valueY = rect.pos.y + std::max(0.0f, (rect.size.y - valueSize.y) * 0.5f) + style.textOffsetY;
        const Color labelColor = enabled ? buttonStyle.text : style.disabledText;
        const Color valueColor = enabled ? items[i].valueText : style.disabledText;
        renderer.drawText({labelX, labelY}, label, labelColor, labelScale);
        if (hasValue) {
            renderer.drawText({valueX, valueY}, items[i].value, valueColor, valueScale);
        }
        renderer.popScreenTransform();
    };

    for (int i = 0; i < itemCount; ++i) {
        const bool active = i == selectedIndex || (i == state.focusedIndex && itemEnabled(i));
        if (!active) {
            drawTab(i);
        }
    }
    for (int i = 0; i < itemCount; ++i) {
        const bool active = i == selectedIndex || (i == state.focusedIndex && itemEnabled(i));
        if (active) {
            drawTab(i);
        }
    }
}

}
