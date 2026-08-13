#include "engine/Ui.hpp"

#include "engine/InputHelpGlyph.hpp"
#include "engine/Utf8.hpp"

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

bool backInputConsumedUntilRelease = false;
const std::unordered_map<std::string, float>* menuIconScaleOverrides = nullptr;

std::unordered_map<std::string, UiWindowState> windowStates;

constexpr std::string_view UiSelectionCursorPath = "assets/system/UI_cursor2.png";
constexpr std::string_view UiArrowButtonPath = "assets/system/UI_arrow.png";
constexpr std::string_view UiArrowButtonWidePath = "assets/system/UI_arrowWide.png";
constexpr std::string_view UiMenuIconDir = "assets/others/";
constexpr std::string_view UiMenuIconPrefix = "img_";
constexpr std::string_view UiMenuIconExtension = ".png";
constexpr int UiHeaderTitleScale = 3;
constexpr int UiConfirmDialogTextScale = 2;
constexpr Vec2 UiHeaderTitleOffset{4.0f, -4.0f};
constexpr Color UiHeaderTitleColor{255, 230, 150, 255};
constexpr float UiWindowBodyTextRightInset = 48.0f;
constexpr Vec2 UiSelectionCursorSize{58.0f, 58.0f};
constexpr Vec2 UiSelectionCursorTargetOffset{8.0f, -5.0f};
constexpr float UiSelectionCursorMoveResponsiveness = 18.0f;
constexpr float UiSelectionCursorBobAmplitude = 3.0f;
constexpr float UiSelectionCursorBobSpeed = 3.2f;
constexpr float UiControlSelectedSizeIncrease = 4.0f;
constexpr float UiControlPressedSizeDecrease = 2.0f;
constexpr int UiControlPressDurationFrames = 6;
constexpr float UiHorizontalTabFocusBrightness = 1.16f;
constexpr float UiHorizontalTabPressedBrightness = 0.82f;
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

struct UiControlVisualSelection {
    UiRect rect{};
    bool pressed = false;
};

struct UiControlPressAnimation {
    UiRect rect{};
    int remainingFrames = 0;
    bool seen = false;
};

std::vector<UiNavigationTarget> previousNavigationTargets;
std::vector<UiNavigationTarget> currentNavigationTargets;
std::vector<UiControlVisualSelection> controlVisualSelections;
std::vector<UiControlPressAnimation> controlPressAnimations;
UiRect navigationFocusRect{};
UiNavigationRole activeNavigationFocusRole = UiNavigationRole::Control;
bool navigationHasFocus = false;
bool uiNavigationActive = false;
bool navigationWasActive = false;
int navigationLayer = 0;
int nextNavigationLayer = 0;
bool navigationTargetRegistrationEnabled = true;

constexpr float NavigationRectEpsilon = 0.5f;
constexpr float NavigationDirectionEpsilon = 0.5f;

bool navigationRectsMatch(UiRect left, UiRect right)
{
    return std::abs(left.pos.x - right.pos.x) <= NavigationRectEpsilon &&
        std::abs(left.pos.y - right.pos.y) <= NavigationRectEpsilon &&
        std::abs(left.size.x - right.size.x) <= NavigationRectEpsilon &&
        std::abs(left.size.y - right.size.y) <= NavigationRectEpsilon;
}

void rememberUiControlVisualSelection(UiRect rect, bool pressed = false)
{
    const auto existing = std::find_if(
        controlVisualSelections.begin(),
        controlVisualSelections.end(),
        [&](const UiControlVisualSelection& selection) {
            return navigationRectsMatch(selection.rect, rect);
        });
    if (existing != controlVisualSelections.end()) {
        existing->pressed = existing->pressed || pressed;
        return;
    }
    controlVisualSelections.push_back({rect, pressed});
}

bool hasUiControlVisualSelection(UiRect rect)
{
    return std::any_of(
        controlVisualSelections.begin(),
        controlVisualSelections.end(),
        [&](const UiControlVisualSelection& selection) {
            return navigationRectsMatch(selection.rect, rect);
        });
}

bool isUiControlVisuallyPressed(UiRect rect)
{
    const auto selection = std::find_if(
        controlVisualSelections.begin(),
        controlVisualSelections.end(),
        [&](const UiControlVisualSelection& candidate) {
            return navigationRectsMatch(candidate.rect, rect);
        });
    return selection != controlVisualSelections.end() && selection->pressed;
}

UiControlPressAnimation* findUiControlPressAnimation(UiRect rect)
{
    const auto animation = std::find_if(
        controlPressAnimations.begin(),
        controlPressAnimations.end(),
        [&](const UiControlPressAnimation& candidate) {
            return navigationRectsMatch(candidate.rect, rect);
        });
    return animation != controlPressAnimations.end() ? &*animation : nullptr;
}

void startUiControlPressAnimation(UiRect rect)
{
    UiControlPressAnimation* animation = findUiControlPressAnimation(rect);
    if (animation == nullptr) {
        controlPressAnimations.push_back({rect, UiControlPressDurationFrames, false});
        return;
    }
    animation->remainingFrames = UiControlPressDurationFrames;
}

bool uiControlPressAnimating(UiRect rect)
{
    UiControlPressAnimation* animation = findUiControlPressAnimation(rect);
    if (animation == nullptr || animation->remainingFrames <= 0) {
        return false;
    }
    animation->seen = true;
    return true;
}

float uiControlScaleForSizeDelta(UiRect rect, float sizeDelta)
{
    const float maxDimension = std::max(rect.size.x, rect.size.y);
    if (maxDimension <= 0.0f) {
        return 1.0f;
    }
    const float targetDimension = std::max(1.0f, maxDimension + sizeDelta);
    return targetDimension / maxDimension;
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
    // スライダーの左右入力は値の調整に予約し、空間ナビゲーションでは消費しない。
    if (current.role == UiNavigationRole::Slider && dx != 0) {
        return -1;
    }
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

bool updateUiNavigation(const Input& input)
{
    uiNavigationActive = input.uiNavigationCursorActive();
    if (previousNavigationTargets.empty()) {
        navigationHasFocus = false;
        navigationWasActive = uiNavigationActive;
        return false;
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
            navigationWasActive = uiNavigationActive;
            return true;
        }
    }
    navigationWasActive = uiNavigationActive;
    return false;
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

Vec2 uiHeaderTitlePosition(Renderer& renderer, UiRect panel, UiWindowFrame frame)
{
    const Vec2 titlePadding = uiWindowFrameHasImageTexture(renderer, frame)
        ? ui::ImageWindowHeaderTitlePadding
        : ui::HeaderTitlePadding;
    return uiHeaderRect(panel).pos + titlePadding + UiHeaderTitleOffset;
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

void drawGaugeCapsuleOutlines(
    Renderer& renderer,
    UiRect rect,
    float radius,
    const UiGaugeStyle& style)
{
    const int outerLayers = std::max(0, static_cast<int>(std::ceil(style.outerOutlineWidth)));
    for (int layer = outerLayers; layer >= 1; --layer) {
        const float expansion = std::min(style.outerOutlineWidth, static_cast<float>(layer));
        const UiRect expanded{
            rect.pos - Vec2{expansion, expansion},
            rect.size + Vec2{expansion * 2.0f, expansion * 2.0f},
        };
        drawCapsuleOutline(renderer, expanded, radius + expansion, style.outerOutline);
    }
    drawCapsuleOutline(renderer, rect, radius, style.outline);
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
constexpr float CommandMenuCursorInset = 2.0f;
constexpr float CommandMenuCursorTopExtension = 8.0f;
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
    return static_cast<int>(utf8::codepointCount(text));
}

UiRect commandMenuItemRect(const UiCommandMenuState& state, int index)
{
    const float x = state.panel.pos.x + CommandMenuPaddingX;
    const float y = state.panel.pos.y + CommandMenuPaddingY + static_cast<float>(index) * (CommandMenuItemHeight + CommandMenuItemGap);
    const float w = std::max(0.0f, state.panel.size.x - CommandMenuPaddingX * 2.0f);
    return {{x, y}, {w, CommandMenuItemHeight}};
}

UiRect commandMenuCursorRect(UiRect itemRect)
{
    return {
        {
            itemRect.pos.x + CommandMenuCursorInset,
            itemRect.pos.y + CommandMenuCursorInset - CommandMenuCursorTopExtension,
        },
        {
            std::max(0.0f, itemRect.size.x - CommandMenuCursorInset * 2.0f),
            std::max(0.0f, itemRect.size.y - CommandMenuCursorInset * 2.0f + CommandMenuCursorTopExtension),
        },
    };
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
    utf8::eraseLastCodepoint(text);
}

std::size_t utf8CodepointByteLength(std::string_view text, std::size_t index)
{
    return utf8::decodeCodepoint(text, index).byteLength;
}

struct WrappedUiColoredTextRun {
    std::string text;
    Color color;
};

struct WrappedUiColoredTextLine {
    std::string text;
    std::vector<WrappedUiColoredTextRun> runs;
    float indent = 0.0f;
};

struct WrappedUiColoredTextLayout {
    std::vector<WrappedUiColoredTextLine> lines;
    float height = 0.0f;
};

bool uiColorsEqual(Color lhs, Color rhs)
{
    return lhs.r == rhs.r
        && lhs.g == rhs.g
        && lhs.b == rhs.b
        && lhs.a == rhs.a;
}

void appendWrappedUiColoredText(
    WrappedUiColoredTextLine& line,
    std::string_view text,
    Color color)
{
    if (text.empty()) {
        return;
    }
    line.text.append(text.data(), text.size());
    if (!line.runs.empty() && uiColorsEqual(line.runs.back().color, color)) {
        line.runs.back().text.append(text.data(), text.size());
        return;
    }
    line.runs.push_back({std::string{text}, color});
}

float uiColoredTextAdvance(Renderer& renderer, std::string_view text, int scale)
{
    if (text.empty()) {
        return 0.0f;
    }
#ifdef _WIN32
    constexpr float NativeTextTexturePaddingX = 4.0f;
#else
    constexpr float NativeTextTexturePaddingX = 0.0f;
#endif
    return std::max(0.0f, renderer.measureText(text, scale).x - NativeTextTexturePaddingX);
}

float uiHangingIndentAdvance(Renderer& renderer, std::string_view prefix, int scale)
{
    if (prefix.empty()) {
        return 0.0f;
    }

    // MeasureString includes padding around the whole texture. Comparing the
    // same following glyph removes that padding and leaves the prefix advance
    // used when the prefix and body are rendered as one string.
    constexpr std::string_view ProbeText = "あ";
    std::string prefixedProbe{prefix};
    prefixedProbe.append(ProbeText);
    return std::max(
        0.0f,
        renderer.measureText(prefixedProbe, scale).x - renderer.measureText(ProbeText, scale).x);
}

float uiWrappedTextLineAdvance(Renderer& renderer, int scale)
{
    const float singleLineHeight = renderer.measureText("あ", scale).y;
    const float twoLineHeight = renderer.measureText("あ\nあ", scale).y;
    return std::max(1.0f, twoLineHeight - singleLineHeight);
}

bool appendUiTextInput(std::string& target, std::string_view text, int maxCodepoints)
{
    bool changed = utf8::sanitizeInPlace(target);
    const int limit = std::max(0, maxCodepoints);
    int count = utf8CodepointCount(target);
    for (std::size_t i = 0; i < text.size();) {
        const utf8::DecodedCodepoint decoded = utf8::decodeCodepoint(text, i);
        if (!decoded.valid) {
            i += decoded.byteLength;
            continue;
        }
        if (decoded.value < 0x20u || (decoded.value >= 0x7fu && decoded.value <= 0x9fu)) {
            i += decoded.byteLength;
            continue;
        }
        if (limit > 0 && count >= limit) {
            break;
        }
        target.append(text.substr(i, decoded.byteLength));
        ++count;
        changed = true;
        i += decoded.byteLength;
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
    navigationLayer = ++nextNavigationLayer;
}

UiNavigationLayerScope::~UiNavigationLayerScope()
{
    navigationLayer = previousLayer_;
}

UiModalNavigationScope::UiModalNavigationScope(UiRect modalRect)
    : previousLayer_(navigationLayer)
{
    navigationLayer = ++nextNavigationLayer;
    const bool previousRegistrationEnabled = navigationTargetRegistrationEnabled;
    navigationTargetRegistrationEnabled = true;
    registerUiNavigationTarget(modalRect, UiNavigationRole::Control, false, false);
    navigationTargetRegistrationEnabled = previousRegistrationEnabled;
}

UiModalNavigationScope::~UiModalNavigationScope()
{
    navigationLayer = previousLayer_;
}

UiExclusiveNavigationScope::UiExclusiveNavigationScope(UiRect modalRect)
    : previousLayer_(navigationLayer)
    , previousRegistrationEnabled_(navigationTargetRegistrationEnabled)
{
    navigationLayer = ++nextNavigationLayer;
    navigationTargetRegistrationEnabled = true;
    registerUiNavigationTarget(modalRect, UiNavigationRole::Control, false, false);
    navigationTargetRegistrationEnabled = false;
    suppressUiSelectionCursor();
}

UiExclusiveNavigationScope::~UiExclusiveNavigationScope()
{
    navigationTargetRegistrationEnabled = previousRegistrationEnabled_;
    navigationLayer = previousLayer_;
}

void registerUiNavigationTarget(UiRect rect, UiNavigationRole role, bool preferred, bool enabled)
{
    if (!navigationTargetRegistrationEnabled) {
        return;
    }
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
    if (!navigationTargetRegistrationEnabled) {
        return;
    }
    if (uiNavigationActive &&
        navigationHasFocus &&
        !navigationRectsMatch(navigationFocusRect, rect)) {
        return;
    }
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

void drawUiTextWithIcon(
    Renderer& renderer,
    Vec2 pos,
    std::string_view text,
    int iconImageNumber,
    Color textColor,
    int textScale,
    float iconSize,
    float iconTextGap,
    Color iconTint)
{
    const Vec2 textSize = renderer.measureText(text, textScale);
    if (iconImageNumber <= 0 || iconSize <= 0.0f) {
        renderer.drawText(pos, text, textColor, textScale);
        return;
    }

    const float scaledIconSize = iconSize * uiMenuIconScale(iconImageNumber);
    const float iconSlotSize = std::max(iconSize, scaledIconSize);
    drawUiIconImage(
        renderer,
        iconImageNumber,
        {pos.x + iconSlotSize * 0.5f, pos.y + textSize.y * 0.5f},
        scaledIconSize,
        iconTint);
    renderer.drawText(
        {pos.x + iconSlotSize + std::max(0.0f, iconTextGap), pos.y},
        text,
        textColor,
        textScale);
}

bool UiRect::contains(Vec2 point) const
{
    return point.x >= pos.x &&
        point.y >= pos.y &&
        point.x < pos.x + size.x &&
        point.y < pos.y + size.y;
}

UiContext::UiContext(const Input& input, Renderer& renderer)
    : renderer_(renderer)
    , mouse_(input.mouseScreen())
    , mouseLeftPressed_(input.mouseLeftPressed())
    , mouseLeftHeld_(input.mouseLeftHeld())
    , pointerActive_(input.lastInputModality() == InputModality::Mouse)
    , navigationConfirmPressed_(input.confirmPressed() || input.useItemPressed())
    , navigationConfirmHeld_(
          input.held(InputAction::Confirm) || input.held(InputAction::UseSelectedItem))
{
    controlVisualSelections.clear();
    for (UiControlPressAnimation& animation : controlPressAnimations) {
        animation.remainingFrames = std::max(0, animation.remainingFrames - 1);
        animation.seen = false;
    }
    if (updateUiNavigation(input)) {
        emitSound(UiSoundEvent::CursorMove);
    }
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

    const int errorIndex = static_cast<int>(UiSoundEvent::Error);
    if (event == UiSoundEvent::Error) {
        // 実行不能のフィードバックは、その操作に先行して予約された決定音なども含めて
        // ブザー音だけに統一する。
        for (int& count : soundEventCounts_) {
            count = 0;
        }
        soundEventCounts_[errorIndex] = 1;
        return;
    }
    if (soundEventCounts_[errorIndex] > 0) {
        return;
    }
    ++soundEventCounts_[index];
}

void UiContext::emitActionResult(bool succeeded, UiSoundEvent successEvent)
{
    emitSound(succeeded ? successEvent : UiSoundEvent::Error);
}

void UiContext::rejectAction()
{
    emitSound(UiSoundEvent::Error);
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
    nextNavigationLayer = 0;
    navigationTargetRegistrationEnabled = true;
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
        focusedIndex >= 0 &&
        !selectionCursor.targetThisFrame) {
        setUiSelectionCursorTarget(previousNavigationTargets[static_cast<std::size_t>(focusedIndex)].rect);
    }
    drawUiSelectionCursor(renderer);
    controlVisualSelections.clear();
    std::erase_if(
        controlPressAnimations,
        [](const UiControlPressAnimation& animation) {
            return !animation.seen || animation.remainingFrames <= 0;
        });
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
{
    (void)state;
}

UiCancelControlScope::~UiCancelControlScope() = default;

UiControlMotionScope::UiControlMotionScope(
    Renderer& renderer,
    UiRect rect,
    UiControlMotion motion,
    bool enabled)
    : UiControlMotionScope(renderer, rect, rect, motion, enabled)
{
}

UiControlMotionScope::UiControlMotionScope(
    Renderer& renderer,
    UiRect visualRect,
    UiRect controlRect,
    UiControlMotion motion,
    bool enabled)
    : renderer_(&renderer)
{
    if (!enabled) {
        return;
    }
    const UiControlVisualState visual = uiControlVisualState(controlRect);
    const float scale = visual.pressed
        ? uiControlScaleForSizeDelta(visualRect, -UiControlPressedSizeDecrease)
        : (motion == UiControlMotion::HoverAndPress && visual.selected
            ? uiControlScaleForSizeDelta(visualRect, UiControlSelectedSizeIncrease)
            : 1.0f);
    if (std::abs(scale - 1.0f) <= 0.0001f) {
        return;
    }
    renderer_->pushScreenTransform(visualRect.pos + visualRect.size * 0.5f, scale, 1.0f);
    transformed_ = true;
}

UiControlVisualState uiControlVisualState(UiRect rect)
{
    return {
        .selected = hasUiControlVisualSelection(rect),
        .pressed = isUiControlVisuallyPressed(rect) || uiControlPressAnimating(rect),
    };
}

UiControlMotionScope::~UiControlMotionScope()
{
    if (renderer_ != nullptr && transformed_) {
        renderer_->popScreenTransform();
    }
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
    return pressed(rect, rect);
}

bool UiContext::pressed(UiRect hitRect, UiRect controlRect)
{
    const bool pointerHit = hovered(hitRect);
    const bool navigationHit = navigationFocused(controlRect);
    if (pointerHit || navigationHit) {
        rememberUiControlVisualSelection(
            controlRect,
            (pointerHit && mouseLeftHeld_) || (navigationHit && navigationConfirmHeld_));
    }
    return pressedWithPointerHit(controlRect, pointerHit);
}

bool UiContext::pressedImage(
    UiRect rect,
    std::string_view imagePath,
    const ImageDrawOptions& options,
    unsigned char alphaThreshold,
    TextureFilter filter)
{
    bool pointerHit = false;
    if (pointerActive_ && !pointerConsumed_) {
        const ImageHandle handle = renderer_.acquireImage(imagePath, filter);
        pointerHit = handle.valid() && renderer_.imageHitTestAlpha(
            handle,
            rect.pos + rect.size * 0.5f,
            rect.size,
            mouse_,
            options,
            alphaThreshold);
    }
    const bool navigationHit = navigationFocused(rect);
    if (pointerHit || navigationHit) {
        rememberUiControlVisualSelection(
            rect,
            (pointerHit && mouseLeftHeld_) || (navigationHit && navigationConfirmHeld_));
    }
    return pressedWithPointerHit(rect, pointerHit);
}

bool UiContext::pressedWithPointerHit(UiRect rect, bool pointerHit)
{
    if (pointerHit && mouseLeftPressed_) {
        startUiControlPressAnimation(rect);
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
        startUiControlPressAnimation(rect);
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

void UiContext::setNavigationFocus(UiRect rect)
{
    navigationFocusRect = rect;
    const int targetIndex = enabledNavigationTargetIndexForRect(previousNavigationTargets, rect);
    activeNavigationFocusRole = targetIndex >= 0
        ? previousNavigationTargets[static_cast<std::size_t>(targetIndex)].role
        : UiNavigationRole::Control;
    navigationHasFocus = true;
}

bool UiContext::selectionFocused(UiRect rect) const
{
    return selectionFocused(rect, rect);
}

bool UiContext::selectionFocused(UiRect hitRect, UiRect controlRect) const
{
    const bool navigationHit = navigationFocused(controlRect);
    const bool pointerHit = hovered(hitRect);
    if (navigationHit || pointerHit) {
        rememberUiControlVisualSelection(
            controlRect,
            (pointerHit && mouseLeftHeld_) || (navigationHit && navigationConfirmHeld_));
    }
    return navigationHit || pointerHit;
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
        ? ui::FooterSingleLineHeight
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

UiRect uiChoiceWindowRect(
    Vec2 position,
    float width,
    int choiceCount,
    std::string_view helpText,
    const UiChoiceWindowLayout& layout)
{
    const int safeChoiceCount = std::max(0, choiceCount);
    const float choiceGap = std::max(0.0f, layout.choiceGap);
    const float choiceListHeight = safeChoiceCount > 0
        ? static_cast<float>(safeChoiceCount) * ui::ButtonHeight +
            static_cast<float>(safeChoiceCount - 1) * choiceGap
        : 0.0f;
    const float height =
        ui::HeaderHeight +
        ui::PanelPadding +
        std::max(0.0f, layout.choiceTopInset) +
        choiceListHeight +
        std::max(0.0f, layout.footerGap) +
        uiFooterHeight(helpText);
    return uiEnsureDecoratedWindowMinSize({position, {std::max(0.0f, width), height}});
}

UiRect uiChoiceWindowButtonRect(UiRect panel, int index, const UiChoiceWindowLayout& layout)
{
    const UiRect body = uiBodyRect(panel);
    const float horizontalInset = std::max(0.0f, layout.choiceHorizontalInset);
    const float width = std::max(0.0f, body.size.x - horizontalInset * 2.0f);
    const float choicePitch = ui::ButtonHeight + std::max(0.0f, layout.choiceGap);
    return {
        {
            body.pos.x + (body.size.x - width) * 0.5f,
            body.pos.y + std::max(0.0f, layout.choiceTopInset) +
                static_cast<float>(std::max(0, index)) * choicePitch,
        },
        {width, ui::ButtonHeight},
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

UiRect uiFooterActionRowRect(UiRect panel)
{
    return {{
        panel.pos.x + ui::PanelPadding,
        panel.pos.y + panel.size.y - ui::FooterSingleLineHeight - ui::FooterActionGap - ui::ButtonHeight,
    }, {
        std::max(0.0f, panel.size.x - ui::PanelPadding * 2.0f),
        ui::ButtonHeight,
    }};
}

UiRect uiFooterActionButtonRect(UiRect panel, Vec2 size, UiFooterActionAlignment alignment)
{
    const UiRect row = uiFooterActionRowRect(panel);
    size.y = ui::ButtonHeight;

    float x = row.pos.x;
    if (alignment == UiFooterActionAlignment::Center) {
        x = panel.pos.x + (panel.size.x - size.x) * 0.5f;
    } else if (alignment == UiFooterActionAlignment::Right) {
        x = row.pos.x + row.size.x - size.x;
    }
    return {{x, row.pos.y}, size};
}

UiRect uiFooterActionGroupButtonRect(UiRect panel, Vec2 size, int index, int count, float gap)
{
    const UiRect row = uiFooterActionRowRect(panel);
    const int safeCount = std::max(1, count);
    const int safeIndex = std::clamp(index, 0, safeCount - 1);
    const float safeGap = std::max(0.0f, gap);
    size.y = ui::ButtonHeight;
    const float totalWidth = size.x * static_cast<float>(safeCount) +
        safeGap * static_cast<float>(safeCount - 1);
    const float groupX = panel.pos.x + (panel.size.x - totalWidth) * 0.5f;
    return {{groupX + static_cast<float>(safeIndex) * (size.x + safeGap), row.pos.y}, size};
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

bool uiCancelControlRequested(const Input& input, UiContext& ui, UiRect panel)
{
    const bool pointerRequested = ui.pressed(uiCancelButtonRect(panel));
    const bool backRequested = input.backPressed() && !ui.backInputConsumed();
    if (backRequested) {
        ui.consumeBackInput();
    }
    if (pointerRequested || backRequested) {
        ui.emitSound(UiSoundEvent::Cancel);
        return true;
    }
    return false;
}

bool uiCancelRequested(UiCancelControlState& state, const Input& input, UiContext& ui, UiRect panel)
{
    state = {};
    return uiCancelControlRequested(input, ui, panel);
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
    if (!uiWindowFrameHasImageTexture(renderer, frame)) {
        renderer.fillRect(header.pos, header.size, ui::HeaderFill);
    }
    const Vec2 titlePos = uiHeaderTitlePosition(renderer, panel, frame);
    renderer.drawText(titlePos, title, UiHeaderTitleColor, UiHeaderTitleScale);
    renderer.drawText(titlePos + Vec2{1.0f, 0.0f}, title, UiHeaderTitleColor, UiHeaderTitleScale);
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
    const float maxWidth = footer.size.x - textPadding.x * 2.0f - ui::FooterHelpTextOffset.x;
    const std::string fitted = fittedInputHelpText(renderer, std::string(helpText), maxWidth, helpStyle);
    drawInputHelpText(renderer, footer.pos + textPadding + ui::FooterHelpTextOffset, fitted, helpStyle);
}

void drawUiBottomInputHelp(
    Renderer& renderer,
    UiRect safeArea,
    std::string helpText,
    float horizontalInset,
    float bottomInset)
{
    if (helpText.empty() || safeArea.size.x <= 0.0f || safeArea.size.y <= 0.0f) {
        return;
    }

    InputHelpStyle helpStyle;
    helpStyle.text = {232, 232, 238, 235};
    helpStyle.outline = {0, 0, 0, 190};
    helpStyle.scale = 2;
    helpStyle.outlinePx = 4;
    helpStyle.iconHeight = 24.0f;
    helpStyle.outlineEnabled = true;

    const float maxWidth = std::max(120.0f, safeArea.size.x - horizontalInset * 2.0f);
    helpText = fittedInputHelpText(renderer, std::move(helpText), maxWidth, helpStyle);
    const Vec2 textSize = measureInputHelpText(renderer, helpText, helpStyle);
    const Vec2 pos{
        safeArea.pos.x + (safeArea.size.x - textSize.x) * 0.5f,
        safeArea.pos.y + std::max(0.0f, safeArea.size.y - textSize.y - bottomInset),
    };
    drawInputHelpText(renderer, pos, helpText, helpStyle);
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
    UiControlMotionScope motion(renderer, rect, UiControlMotion::HoverAndPress);
    ImageDrawOptions options;
    options.anchor = {0.5f, 0.5f};
    if (renderer.drawImage("assets/system/UI_cancelButton.png", rect.pos + rect.size * 0.5f, rect.size, options)) {
        return;
    }

    renderer.fillRect(rect.pos, rect.size, {30, 24, 42, 230});
    renderer.drawRect(rect.pos, rect.size, ui::WindowBorder);
    renderer.drawLine(rect.pos + Vec2{15.0f, 16.0f}, rect.pos + rect.size - Vec2{15.0f, 16.0f}, ui::Text);
    renderer.drawLine({rect.pos.x + rect.size.x - 15.0f, rect.pos.y + 16.0f}, {rect.pos.x + 15.0f, rect.pos.y + rect.size.y - 16.0f}, ui::Text);
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
        drawGaugeCapsuleOutlines(renderer, rect, radius, style);
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

    const float filledW = rect.size.x * progress;
    if (filledW <= 0.0f) {
        drawGaugeCapsuleOutlines(renderer, rect, radius, style);
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

    drawGaugeCapsuleOutlines(renderer, rect, radius, style);
}

namespace {

float uiSliderThumbRadius(UiRect rect, const UiSliderStyle& style)
{
    return style.thumbRadius >= 0.0f
        ? style.thumbRadius
        : clamp(rect.size.y * 0.45f, 6.0f, 10.0f);
}

UiRect uiSliderThumbRect(
    UiRect rect,
    float value,
    const UiSliderSpec& spec,
    const UiSliderStyle& style)
{
    const float range = spec.maxValue - spec.minValue;
    const float progress = range > 0.0f
        ? clamp((value - spec.minValue) / range, 0.0f, 1.0f)
        : 0.0f;
    const float radius = uiSliderThumbRadius(rect, style);
    const Vec2 center{
        rect.pos.x + rect.size.x * progress,
        rect.pos.y + rect.size.y * 0.5f,
    };
    return {center - Vec2{radius, radius}, {radius * 2.0f, radius * 2.0f}};
}

}

UiSliderResult updateUiSlider(
    UiContext& ui,
    const Input& input,
    UiRect rect,
    float value,
    const UiSliderSpec& spec,
    UiSliderState& state,
    const UiSliderStyle& style)
{
    UiSliderResult result{value, false, false};
    const float range = spec.maxValue - spec.minValue;
    if (range <= 0.0f || rect.size.x <= 0.0f || rect.size.y <= 0.0f) {
        state.dismissValue();
        return result;
    }

    const bool navigationFocused =
        style.navigationEnabled && ui.navigationFocused(rect);
    // 入力領域はゲージ全体のまま維持する。ここで記録した選択状態は、
    // 描画側でつまみだけのモーションへ適用する。
    (void)ui.selectionFocused(rect);

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

    const bool pointerPressedInside = ui.pressed(rect) && !ui.navigationActive();
    const auto otherOperationPressed = [&] {
        if ((input.mouseLeftPressed() && !pointerPressedInside) ||
            input.mouseWheelDelta() != 0 ||
            input.saveShortcutPressed() ||
            input.undoShortcutPressed() ||
            input.redoShortcutPressed() ||
            input.copyShortcutPressed() ||
            input.pasteShortcutPressed() ||
            input.shortcutCursorDelta() != 0 ||
            input.cycleDelta() != 0) {
            return true;
        }
        for (int action = 0; action < InputActionCount; ++action) {
            if (input.pressed(static_cast<InputAction>(action))) {
                return true;
            }
        }
        return false;
    };
    if (!state.dragging() && state.valueBubbleVisible() && otherOperationPressed()) {
        state.dismissValue();
    }

    bool interacting = false;
    if (state.dragging()) {
        if (input.mouseLeftHeld()) {
            interacting = true;
            rememberUiControlVisualSelection(rect, true);
        } else {
            state.releaseDrag();
        }
    } else if (pointerPressedInside) {
        state.beginDrag();
        interacting = true;
    }

    if (interacting) {
        state.beginDrag();
        result.value = valueAtPointer();
        result.changed = std::abs(result.value - value) > 0.0001f;
        result.interacting = true;
        ui.consumePointer();
    }
    if (navigationFocused && !interacting) {
        const int direction =
            (input.pressed(InputAction::MoveRight) ? 1 : 0) -
            (input.pressed(InputAction::MoveLeft) ? 1 : 0);
        if (direction != 0) {
            const float step = spec.step > 0.0f ? spec.step : range * 0.01f;
            result.value = clamp(
                value + step * static_cast<float>(direction),
                spec.minValue,
                spec.maxValue);
            result.changed = std::abs(result.value - value) > 0.0001f;
            result.interacting = true;
            state.showValue();
            ui.setNavigationFocus(rect);
            (void)ui.selectionFocused(rect);
            if (result.changed) {
                ui.emitSound(UiSoundEvent::Confirm);
            }
        }
    }
    return result;
}

void drawUiSlider(
    Renderer& renderer,
    UiRect rect,
    float value,
    const UiSliderSpec& spec,
    const UiSliderState& state,
    const UiSliderStyle& style)
{
    const float range = spec.maxValue - spec.minValue;
    if (range <= 0.0f || rect.size.x <= 0.0f || rect.size.y <= 0.0f) {
        return;
    }

    const float progress = clamp((value - spec.minValue) / range, 0.0f, 1.0f);
    const float centerY = rect.pos.y + rect.size.y * 0.5f;
    const float trackThickness = clamp(
        style.trackThickness,
        1.0f,
        std::max(1.0f, rect.size.y));
    const UiRect track{{
        rect.pos.x,
        centerY - trackThickness * 0.5f,
    }, {
        rect.size.x,
        trackThickness,
    }};
    fillRoundedRect(renderer, track, trackThickness * 0.5f, style.track);

    const float thumbX = rect.pos.x + rect.size.x * progress;
    const float activeWidth = std::max(0.0f, thumbX - rect.pos.x);
    if (activeWidth > 0.0f) {
        fillRoundedRect(
            renderer,
            {track.pos, {activeWidth, track.size.y}},
            trackThickness * 0.5f,
            style.activeTrack);
    }

    const float thumbRadius = uiSliderThumbRadius(rect, style);
    const Vec2 thumbCenter{thumbX, centerY};
    const UiRect thumbRect = uiSliderThumbRect(rect, value, spec, style);
    if (style.navigationEnabled) {
        registerUiNavigationTarget(rect, UiNavigationRole::Slider, false);
    }
    {
        UiControlMotionScope thumbMotion(
            renderer,
            thumbRect,
            rect,
            UiControlMotion::HoverAndPress);
        renderer.fillSoftCircle(thumbCenter, thumbRadius, style.thumb);
        if (colorVisible(style.thumbOutline)) {
            renderer.drawSoftRing(thumbCenter, thumbRadius, 1.25f, style.thumbOutline);
        }
    }
    if (style.navigationEnabled && uiControlVisualState(rect).selected) {
        requestUiSelectionCursor(thumbRect);
    }

    if (state.valueBubbleVisible() && style.showValueBubble && colorVisible(style.valueBubble)) {
        char numericValueBuffer[32];
        std::snprintf(
            numericValueBuffer,
            sizeof(numericValueBuffer),
            "%.*f",
            std::clamp(spec.valueDecimalPlaces, 0, 4),
            clamp(value, spec.minValue, spec.maxValue));
        std::string valueText{numericValueBuffer};
        valueText.append(spec.valueSuffix);
        constexpr int ValueTextScale = 1;
        constexpr float BubblePaddingX = 8.0f;
        constexpr float BubblePaddingY = 5.0f;
        constexpr float PointerHeight = 5.0f;
        constexpr float BubbleGap = 3.0f;
        const Vec2 textSize = renderer.measureText(valueText, ValueTextScale);
        const Vec2 bubbleSize{
            std::max(30.0f, textSize.x + BubblePaddingX * 2.0f),
            textSize.y + BubblePaddingY * 2.0f,
        };
        const UiRect bubble{{
            thumbCenter.x - bubbleSize.x * 0.5f,
            thumbCenter.y - thumbRadius - BubbleGap - PointerHeight - bubbleSize.y,
        }, bubbleSize};
        fillRoundedRect(renderer, bubble, 2.0f, style.valueBubble);

        const Vec2 pointer[] = {
            {thumbCenter.x - PointerHeight, bubble.pos.y + bubble.size.y - 1.0f},
            {thumbCenter.x + PointerHeight, bubble.pos.y + bubble.size.y - 1.0f},
            {thumbCenter.x, bubble.pos.y + bubble.size.y + PointerHeight},
        };
        renderer.fillPolygon(pointer, 3, style.valueBubble);
        renderer.drawText(
            {
                bubble.pos.x + (bubble.size.x - textSize.x) * 0.5f,
                bubble.pos.y + (bubble.size.y - textSize.y) * 0.5f,
            },
            valueText,
            style.valueText,
            ValueTextScale);
    }
}

void drawUiButton(Renderer& renderer, UiRect rect, std::string_view label, bool preferred, const UiButtonStyle& style)
{
    drawUiButton(renderer, rect, label, preferred, UiButtonState::Enabled, style);
}

bool tryActivateUiButton(UiContext& ui, UiButtonState state)
{
    if (uiButtonAvailable(state)) {
        return true;
    }
    ui.rejectAction();
    return false;
}

UiButtonStyle uiButtonStyleForState(UiButtonStyle style, UiButtonState state)
{
    if (uiButtonAvailable(state)) {
        return style;
    }

    constexpr float UnavailableAlpha = 0.5f;
    style.fill = alphaScaledColor(style.fill, UnavailableAlpha);
    style.fillHot = alphaScaledColor(style.fillHot, UnavailableAlpha);
    style.outline = alphaScaledColor(style.outline, UnavailableAlpha);
    style.outlineHot = alphaScaledColor(style.outlineHot, UnavailableAlpha);
    style.text = alphaScaledColor(style.text, UnavailableAlpha);
    style.imageTint = alphaScaledColor(style.imageTint, UnavailableAlpha);
    style.imageTintHot = alphaScaledColor(style.imageTintHot, UnavailableAlpha);
    return style;
}

void drawUiButton(
    Renderer& renderer,
    UiRect rect,
    std::string_view label,
    bool preferred,
    UiButtonState state,
    const UiButtonStyle& style)
{
    drawUiButton(renderer, rect, label, preferred, state, style, UiNavigationRole::Control);
}

namespace {

void drawUiButtonWithNavigationRole(
    Renderer& renderer,
    UiRect rect,
    std::string_view label,
    int iconImageNumber,
    bool preferred,
    const UiButtonStyle& style,
    UiNavigationRole navigationRole,
    bool enabled)
{
    rect.size.y = ui::ButtonHeight;
    const bool selected = uiControlVisualState(rect).selected;
    registerUiNavigationTarget(rect, navigationRole, preferred, enabled);
    UiControlMotionScope motion(renderer, rect, UiControlMotion::PressOnly, enabled);

    if (renderer.hasUiButtonTexture()) {
        Color tint = selected ? style.imageTintHot : style.imageTint;
        renderer.drawUiButtonFrame(rect.pos, rect.size.x, style.imageVariant, selected, tint);
    } else {
        Color fill = selected ? style.fillHot : style.fill;
        Color outline = selected ? scaledColor(style.outlineHot, 1.04f) : style.outline;
        renderer.fillRect(rect.pos, rect.size, fill);
        renderer.drawRect(rect.pos, rect.size, outline);
    }

    drawUiLabelWithOptionalIcon(
        renderer,
        rect,
        label,
        iconImageNumber,
        style.text,
        2,
        UiButtonIconSize,
        ui::ButtonTextPadding.y);
    if (selected) {
        requestUiSelectionCursor(rect);
    }
}

}

void drawUiButton(
    Renderer& renderer,
    UiRect rect,
    std::string_view label,
    bool preferred,
    const UiButtonStyle& style,
    UiNavigationRole navigationRole)
{
    drawUiButton(renderer, rect, label, preferred, UiButtonState::Enabled, style, navigationRole);
}

void drawUiButton(
    Renderer& renderer,
    UiRect rect,
    std::string_view label,
    bool preferred,
    UiButtonState state,
    const UiButtonStyle& style,
    UiNavigationRole navigationRole)
{
    drawUiButtonWithNavigationRole(
        renderer,
        rect,
        label,
        0,
        preferred,
        uiButtonStyleForState(style, state),
        navigationRole,
        uiButtonAvailable(state));
}

void drawUiButton(Renderer& renderer, UiRect rect, std::string_view label, int iconImageNumber, bool preferred, const UiButtonStyle& style)
{
    drawUiButton(renderer, rect, label, iconImageNumber, preferred, UiButtonState::Enabled, style);
}

void drawUiButton(
    Renderer& renderer,
    UiRect rect,
    std::string_view label,
    int iconImageNumber,
    bool preferred,
    UiButtonState state,
    const UiButtonStyle& style)
{
    drawUiButtonWithNavigationRole(
        renderer,
        rect,
        label,
        iconImageNumber,
        preferred,
        uiButtonStyleForState(style, state),
        UiNavigationRole::Control,
        uiButtonAvailable(state));
}

namespace {

bool drawUiFlexibleButtonFrameContent(
    Renderer& renderer,
    UiRect rect,
    bool preferred,
    UiButtonState state,
    const UiButtonStyle& style,
    bool visuallySelected)
{
    const UiButtonStyle resolvedStyle = uiButtonStyleForState(style, state);
    const bool interactionSelected = uiControlVisualState(rect).selected;
    registerUiNavigationTarget(
        rect,
        UiNavigationRole::Control,
        preferred,
        uiButtonAvailable(state));
    const Color tint = visuallySelected ? resolvedStyle.imageTintHot : resolvedStyle.imageTint;
    if (!drawUiFlexibleButtonImage(renderer, rect, visuallySelected, tint)) {
        const Color fill = visuallySelected ? resolvedStyle.fillHot : resolvedStyle.fill;
        const Color outline = visuallySelected ? scaledColor(resolvedStyle.outlineHot, 1.04f) : resolvedStyle.outline;
        renderer.fillRect(rect.pos, rect.size, fill);
        renderer.drawRect(rect.pos, rect.size, outline);
    }
    if (interactionSelected) {
        requestUiSelectionCursor(rect);
    }
    return interactionSelected;
}

}

void drawUiFlexibleButtonFrame(Renderer& renderer, UiRect rect, bool preferred, const UiButtonStyle& style)
{
    drawUiFlexibleButtonFrame(renderer, rect, preferred, UiButtonState::Enabled, style);
}

void drawUiFlexibleButtonFrame(
    Renderer& renderer,
    UiRect rect,
    bool preferred,
    UiButtonState state,
    const UiButtonStyle& style)
{
    UiControlMotionScope motion(renderer, rect, UiControlMotion::PressOnly, uiButtonAvailable(state));
    drawUiFlexibleButtonFrameContent(
        renderer,
        rect,
        preferred,
        state,
        style,
        uiControlVisualState(rect).selected);
}

void drawUiFlexibleButtonFrameWithVisualSelection(
    Renderer& renderer,
    UiRect rect,
    bool preferred,
    bool visuallySelected,
    const UiButtonStyle& style)
{
    drawUiFlexibleButtonFrameContent(
        renderer,
        rect,
        preferred,
        UiButtonState::Enabled,
        style,
        visuallySelected);
}

void drawUiFlexibleButton(Renderer& renderer, UiRect rect, std::string_view label, bool preferred, const UiButtonStyle& style)
{
    drawUiFlexibleButton(renderer, rect, label, preferred, UiButtonState::Enabled, style);
}

void drawUiFlexibleButton(
    Renderer& renderer,
    UiRect rect,
    std::string_view label,
    bool preferred,
    UiButtonState state,
    const UiButtonStyle& style)
{
    const UiButtonStyle resolvedStyle = uiButtonStyleForState(style, state);
    UiControlMotionScope motion(renderer, rect, UiControlMotion::PressOnly, uiButtonAvailable(state));
    drawUiFlexibleButtonFrameContent(
        renderer,
        rect,
        preferred,
        state,
        style,
        uiControlVisualState(rect).selected);

    const Vec2 textSize = renderer.measureText(label, 2);
    const Vec2 textPos{
        rect.pos.x + std::max(0.0f, (rect.size.x - textSize.x) * 0.5f),
        rect.pos.y + std::max(0.0f, (rect.size.y - textSize.y) * 0.5f),
    };
    renderer.drawText(textPos, label, resolvedStyle.text, 2);
}

void drawUiRectButton(Renderer& renderer, UiRect rect, std::string_view label, bool hot, const UiButtonStyle& style)
{
    drawUiRectButton(renderer, rect, label, hot, UiButtonState::Enabled, style);
}

void drawUiRectButton(
    Renderer& renderer,
    UiRect rect,
    std::string_view label,
    bool hot,
    UiButtonState state,
    const UiButtonStyle& style)
{
    const UiButtonStyle resolvedStyle = uiButtonStyleForState(style, state);
    const bool selected = uiControlVisualState(rect).selected;
    registerUiNavigationTarget(
        rect,
        UiNavigationRole::Control,
        hot,
        uiButtonAvailable(state));
    UiControlMotionScope motion(renderer, rect, UiControlMotion::PressOnly, uiButtonAvailable(state));

    const Color fill = selected ? resolvedStyle.fillHot : resolvedStyle.fill;
    const Color outline = selected ? scaledColor(resolvedStyle.outlineHot, 1.04f) : resolvedStyle.outline;
    renderer.fillRect(rect.pos, rect.size, fill);
    renderer.drawRect(rect.pos, rect.size, outline);

    const Vec2 textSize = renderer.measureText(label, 2);
    const Vec2 textPos{
        rect.pos.x + std::max(0.0f, (rect.size.x - textSize.x) * 0.5f),
        rect.pos.y + std::max(0.0f, (rect.size.y - textSize.y) * 0.5f),
    };
    renderer.drawText(textPos, label, resolvedStyle.text, 2);
    if (selected) {
        requestUiSelectionCursor(rect);
    }
}

namespace {

std::string_view uiArrowButtonImagePath(UiArrowButtonVariant variant)
{
    return variant == UiArrowButtonVariant::Wide ? UiArrowButtonWidePath : UiArrowButtonPath;
}

float uiArrowButtonRotationDegrees(UiArrowDirection direction)
{
    switch (direction) {
    case UiArrowDirection::Up:
        return 0.0f;
    case UiArrowDirection::Right:
        return 90.0f;
    case UiArrowDirection::Down:
        return 180.0f;
    case UiArrowDirection::Left:
        return 270.0f;
    }
    return 0.0f;
}

UiRect normalizedUiArrowButtonRect(UiRect rect, UiArrowButtonVariant variant)
{
    const Vec2 nativeSize = uiArrowButtonNativeSize(variant);
    const Vec2 center = rect.pos + rect.size * 0.5f;
    return {center - nativeSize * 0.5f, nativeSize};
}

ImageDrawOptions uiArrowButtonImageOptions(UiArrowDirection direction, bool enabled)
{
    ImageDrawOptions options;
    options.rotationDegrees = uiArrowButtonRotationDegrees(direction);
    if (!enabled) {
        options.tint = {148, 148, 156, 118};
    }
    return options;
}

} // namespace

Vec2 uiArrowButtonNativeSize(UiArrowButtonVariant variant)
{
    return variant == UiArrowButtonVariant::Wide ? ui::ArrowButtonWideSize : ui::ArrowButtonSize;
}

bool updateUiArrowButton(
    UiContext& ui,
    UiRect rect,
    UiArrowDirection direction,
    UiArrowButtonVariant variant,
    bool enabled)
{
    const UiRect imageRect = normalizedUiArrowButtonRect(rect, variant);
    const bool activated = ui.pressedImage(
        imageRect,
        uiArrowButtonImagePath(variant),
        uiArrowButtonImageOptions(direction, enabled));
    if (!activated) {
        return false;
    }
    if (!enabled) {
        ui.rejectAction();
        return false;
    }
    return true;
}

void drawUiArrowButton(
    Renderer& renderer,
    UiRect rect,
    UiArrowDirection direction,
    UiArrowButtonVariant variant,
    bool enabled)
{
    const UiRect imageRect = normalizedUiArrowButtonRect(rect, variant);
    registerUiNavigationTarget(imageRect, UiNavigationRole::Control, false, enabled);
    UiControlMotionScope motion(renderer, imageRect, UiControlMotion::HoverAndPress, enabled);
    renderer.drawImage(
        uiArrowButtonImagePath(variant),
        imageRect.pos + imageRect.size * 0.5f,
        imageRect.size,
        uiArrowButtonImageOptions(direction, enabled),
        TextureFilter::Nearest);
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
    const bool selected = uiControlVisualState(rect).selected;
    registerUiNavigationTarget(rect, UiNavigationRole::Control, hot, !disabled);
    UiControlMotionScope motion(renderer, rect, UiControlMotion::PressOnly, !disabled);
    const Color fill = disabled ? style.fillDisabled : (selected ? style.fillHot : style.fill);
    const Color outline = disabled ? style.outlineDisabled : (selected ? style.outlineHot : style.outline);
    const Color labelColor = disabled ? style.disabledText : style.text;
    const Color valueColor = disabled ? style.disabledText : style.valueText;
    renderer.fillRect(rect.pos, rect.size, fill);
    renderer.drawRect(rect.pos, rect.size, outline);
    if (selected && !disabled) {
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
        state.composition = event.edit.text == nullptr
            ? std::string{}
            : utf8::sanitized(event.edit.text);
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
    if (!ui::SystemMessagesVisible || message.empty()) {
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

WrappedUiColoredTextLayout layoutUiWrappedColoredText(
    Renderer& renderer,
    std::span<const UiColoredTextRun> runs,
    float maxWidth,
    const UiWrappedColoredTextStyle& style)
{
    WrappedUiColoredTextLayout layout;
    WrappedUiColoredTextLine line;
    const float wrapWidth = std::max(1.0f, maxWidth);
    const int scale = std::max(1, style.scale);
    const float hangingIndent = std::clamp(
        uiHangingIndentAdvance(renderer, style.hangingIndentText, scale),
        0.0f,
        std::max(0.0f, wrapWidth - 1.0f));

    for (const UiColoredTextRun& run : runs) {
        for (std::size_t i = 0; i < run.text.size();) {
            if (run.text[i] == '\n') {
                layout.lines.push_back(std::move(line));
                line = {};
                ++i;
                continue;
            }

            const std::size_t length = utf8CodepointByteLength(run.text, i);
            if (length == 0) {
                break;
            }
            const std::string_view token = run.text.substr(i, length);
            std::string candidate = line.text;
            candidate.append(token.data(), token.size());
            if (!line.text.empty() &&
                line.indent + uiColoredTextAdvance(renderer, candidate, scale) > wrapWidth) {
                layout.lines.push_back(std::move(line));
                line = {};
                line.indent = hangingIndent;
            }
            appendWrappedUiColoredText(line, token, run.color);
            i += length;
        }
    }

    if (!line.text.empty()) {
        layout.lines.push_back(std::move(line));
    }
    if (layout.lines.empty()) {
        return layout;
    }

    const float singleLineHeight = renderer.measureText("あ", scale).y;
    const float lineAdvance = uiWrappedTextLineAdvance(renderer, scale);
    layout.height = singleLineHeight + lineAdvance * static_cast<float>(layout.lines.size() - 1);
    return layout;
}

[[nodiscard]] float measureUiWrappedColoredText(
    Renderer& renderer,
    std::span<const UiColoredTextRun> runs,
    float maxWidth,
    const UiWrappedColoredTextStyle& style)
{
    return layoutUiWrappedColoredText(renderer, runs, maxWidth, style).height;
}

[[nodiscard]] float drawUiWrappedColoredText(
    Renderer& renderer,
    Vec2 pos,
    std::span<const UiColoredTextRun> runs,
    float maxWidth,
    int scale)
{
    UiWrappedColoredTextStyle style;
    style.scale = scale;
    return drawUiWrappedColoredText(renderer, pos, runs, maxWidth, style);
}

[[nodiscard]] float drawUiWrappedColoredText(
    Renderer& renderer,
    Vec2 pos,
    std::span<const UiColoredTextRun> runs,
    float maxWidth,
    const UiWrappedColoredTextStyle& style)
{
    const WrappedUiColoredTextLayout layout = layoutUiWrappedColoredText(renderer, runs, maxWidth, style);
    const int scale = std::max(1, style.scale);
    const float lineAdvance = uiWrappedTextLineAdvance(renderer, scale);
    float y = pos.y;
    for (const WrappedUiColoredTextLine& wrappedLine : layout.lines) {
        float x = pos.x + wrappedLine.indent;
        for (const WrappedUiColoredTextRun& run : wrappedLine.runs) {
            renderer.drawText({x, y}, run.text, run.color, scale);
            x += uiColoredTextAdvance(renderer, run.text, scale);
        }
        y += lineAdvance;
    }
    return layout.height;
}

namespace {

float drawUiDetailHeaderImpl(
    Renderer& renderer,
    UiRect panel,
    std::string_view text,
    int iconImageNumber)
{
    constexpr float MinHeaderHeight = 50.0f;
    constexpr float HeaderGap = 16.0f;
    constexpr float IconSize = 40.0f;
    constexpr float IconTextGap = 8.0f;
    constexpr int TextScale = 3;
    const UiRect content = uiSubPanelContentRect(panel);
    const bool hasIcon = iconImageNumber > 0;
    const float scaledIconSize = hasIcon ? IconSize * uiMenuIconScale(iconImageNumber) : 0.0f;
    const float iconSlotSize = hasIcon ? std::max(IconSize, scaledIconSize) : 0.0f;
    const float iconGap = hasIcon ? IconTextGap : 0.0f;
    const Vec2 textPos{content.pos.x + iconSlotSize + iconGap, content.pos.y};
    const float textWidth = std::max(1.0f, content.size.x - iconSlotSize - iconGap);
    const float textHeight = renderer.measureWrappedText(text, textWidth, TextScale).y;
    const float headerContentHeight = std::max(textHeight, iconSlotSize);
    const float textY = hasIcon
        ? content.pos.y + std::max(0.0f, (headerContentHeight - textHeight) * 0.5f)
        : content.pos.y;

    if (hasIcon) {
        drawUiIconImage(
            renderer,
            iconImageNumber,
            {content.pos.x + iconSlotSize * 0.5f, content.pos.y + headerContentHeight * 0.5f},
            scaledIconSize,
            {255, 255, 255, 255});
    }
    renderer.drawWrappedText({textPos.x, textY}, text, textWidth, ui::Text, TextScale);
    renderer.drawWrappedText({textPos.x + 1.0f, textY}, text, textWidth, ui::Text, TextScale);
    return content.pos.y + std::max(MinHeaderHeight, headerContentHeight + HeaderGap);
}

}

float drawUiDetailHeader(Renderer& renderer, UiRect panel, std::string_view text)
{
    return drawUiDetailHeaderImpl(renderer, panel, text, 0);
}

float drawUiDetailHeaderWithIcon(
    Renderer& renderer,
    UiRect panel,
    std::string_view text,
    int iconImageNumber)
{
    return drawUiDetailHeaderImpl(renderer, panel, text, iconImageNumber);
}

void drawUiDetailText(Renderer& renderer, UiRect panel, float& y, std::string_view text)
{
    drawUiDetailText(renderer, panel, y, text, UiDetailTextStyle{});
}

void drawUiDetailText(Renderer& renderer, UiRect panel, float& y, std::string_view text, Color color)
{
    UiDetailTextStyle style;
    style.color = color;
    drawUiDetailText(renderer, panel, y, text, style);
}

void drawUiDetailText(
    Renderer& renderer,
    UiRect panel,
    float& y,
    std::string_view text,
    const UiDetailTextStyle& style)
{
    const UiRect content = uiSubPanelContentRect(panel);
    const int scale = std::max(1, style.scale);
    renderer.drawWrappedText({content.pos.x, y}, text, content.size.x, style.color, scale);
    y += renderer.measureWrappedText(text, content.size.x, scale).y + std::max(0.0f, style.bottomGap);
}

void drawUiDetailLine(Renderer& renderer, UiRect panel, float& y, std::string_view label, std::string_view value, Color valueColor)
{
    UiDetailLineStyle style;
    style.valueColor = valueColor;
    drawUiDetailLine(renderer, panel, y, label, value, style);
}

void drawUiDetailLine(
    Renderer& renderer,
    UiRect panel,
    float& y,
    std::string_view label,
    std::string_view value,
    const UiDetailLineStyle& style)
{
    const float labelX = panel.pos.x + ui::SubPanelPadding.x;
    const float valueX = labelX + std::max(0.0f, style.labelWidth);
    const float valueMaxWidth = panel.pos.x + panel.size.x - valueX - ui::SubPanelPadding.x;
    const int scale = std::max(1, style.scale);
    renderer.drawText({labelX, y}, label, style.labelColor, scale);
    renderer.drawWrappedText({valueX, y}, value, valueMaxWidth, style.valueColor, scale);
    const float contentHeight = std::max(
        renderer.measureText(label, scale).y,
        renderer.measureWrappedText(value, valueMaxWidth, scale).y);
    y += std::max(
        std::max(0.0f, style.minLineHeight),
        contentHeight + std::max(0.0f, style.lineGap));
}

UiRect uiResultDialogOkButtonRect(UiRect panel)
{
    return uiFooterActionButtonRect(
        panel,
        {180.0f, ui::ButtonHeight},
        UiFooterActionAlignment::Center);
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

UiRect uiWindowBodyTextRect(
    Renderer& renderer,
    UiRect panel,
    std::string_view title,
    std::string_view text,
    float nextControlTop,
    int textScale,
    UiWindowFrame frame)
{
    const Vec2 titlePos = uiHeaderTitlePosition(renderer, panel, frame);
    const float left = titlePos.x;
    const float right = panel.pos.x + panel.size.x - UiWindowBodyTextRightInset;
    const float width = std::max(0.0f, right - left);
    const float titleBottom = titlePos.y + renderer.measureText(title, UiHeaderTitleScale).y;
    const float textHeight = renderer.measureWrappedText(text, width, textScale).y;
    const float availableHeight = std::max(0.0f, nextControlTop - titleBottom);
    const float top = titleBottom + std::max(0.0f, availableHeight - textHeight) * 0.5f;
    return {{
        left,
        top,
    }, {
        width,
        std::max(0.0f, nextControlTop - top),
    }};
}

void drawUiWindowBodyText(
    Renderer& renderer,
    UiRect panel,
    std::string_view title,
    std::string_view text,
    float nextControlTop,
    Color color,
    int textScale,
    UiWindowFrame frame)
{
    const UiRect bodyText = uiWindowBodyTextRect(
        renderer,
        panel,
        title,
        text,
        nextControlTop,
        textScale,
        frame);
    renderer.drawWrappedText(bodyText.pos, text, bodyText.size.x, color, textScale);
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

UiRect uiQuantityConfirmButtonRect(UiRect panel)
{
    return uiFooterActionButtonRect(
        panel,
        {180.0f, ui::ButtonHeight},
        UiFooterActionAlignment::Center);
}

UiRect uiQuantityControlRect(UiRect panel)
{
    const UiRect body = uiBodyRect(panel, 0.0f, ui::PanelPadding);
    const UiRect confirmButton = uiQuantityConfirmButtonRect(panel);
    return {
        body.pos,
        {
            body.size.x,
            std::max(0.0f, confirmButton.pos.y - ui::ButtonGap - body.pos.y),
        },
    };
}

UiRect uiQuantityDownButtonRect(UiRect panel)
{
    const UiRect control = uiQuantityControlRect(panel);
    constexpr Vec2 ButtonSize = ui::ArrowButtonWideSize;
    return {
        {
            control.pos.x + (control.size.x - ButtonSize.x) * 0.5f,
            control.pos.y + control.size.y - ButtonSize.y,
        },
        ButtonSize,
    };
}

UiRect uiQuantityUpButtonRect(UiRect panel)
{
    const UiRect control = uiQuantityControlRect(panel);
    constexpr Vec2 ButtonSize = ui::ArrowButtonWideSize;
    return {
        {control.pos.x + (control.size.x - ButtonSize.x) * 0.5f, control.pos.y},
        ButtonSize,
    };
}

void closeUiConfirmDialog(UiConfirmDialogState& state)
{
    state.open = false;
    state.title.clear();
    state.message.clear();
    state.confirmLabel = "はい";
    state.cancelLabel = "いいえ";
    state.selection = 1;
    state.confirmState = UiButtonState::Enabled;
}

void closeUiQuantityDialog(UiQuantityDialogState& state)
{
    state.open = false;
    state.title.clear();
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
    panel = fitUiResultDialogRect(state, panel);
    UiModalNavigationScope navigationScope(panel);

    const std::string helpText = buildInputHelpText({
        {InputHelpGroup::Primary, {InputAction::Confirm, InputAction::UseSelectedItem}, "閉じる"},
    });
    UiWindowScope window(renderer, id, panel, "", helpText, UiWindowOptions{true, false});
    const UiRect body = uiResultDialogTextRect(panel);
    float y = body.pos.y;
    constexpr int TextScale = 2;
    for (const UiResultDialogLine& line : state.lines) {
        drawCenteredUiResultDialogLine(renderer, body, y, line, TextScale);
        y += measureUiResultDialogLine(renderer, line, TextScale).y + 10.0f;
    }
    drawUiButton(renderer, uiResultDialogOkButtonRect(panel), "OK", false, uiActionButtonStyle());
}

UiRect uiConfirmDialogButtonRect(UiRect panel, int index)
{
    constexpr Vec2 Size{164.0f, ui::ButtonHeight};
    constexpr int ButtonCount = 2;
    const int visualIndex = std::clamp(index, 0, ButtonCount - 1) == 0 ? 1 : 0;
    return uiFooterActionGroupButtonRect(
        panel,
        Size,
        visualIndex,
        ButtonCount,
        ui::FooterActionPairGap);
}

std::string uiConfirmDialogHelpText()
{
    return buildInputHelpText({
        {InputHelpGroup::Primary, {InputAction::Confirm, InputAction::UseSelectedItem}, "決定"},
        {InputHelpGroup::Back, {InputAction::Cancel, InputAction::Pause}, "戻る"},
    });
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
    state.confirmState = UiButtonState::Enabled;
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
    if (input.pressed(InputAction::MoveLeft) || input.pressed(InputAction::MoveUp)) {
        state.selection = 1;
    }
    if (input.pressed(InputAction::MoveRight) || input.pressed(InputAction::MoveDown)) {
        state.selection = 0;
    }
    ui.emitCursorMoveIfChanged(previousSelection, state.selection);

    const bool confirmRequested =
        ui.pressed(uiConfirmDialogButtonRect(panel, 0)) ||
        ((input.confirmPressed() || input.useItemPressed()) && state.selection == 0);
    const bool cancelControlRequested = uiCancelControlRequested(input, ui, panel);
    const bool cancelRequested =
        ui.pressed(uiConfirmDialogButtonRect(panel, 1)) ||
        cancelControlRequested ||
        ((input.confirmPressed() || input.useItemPressed()) && state.selection == 1);

    if (confirmRequested && tryActivateUiButton(ui, state.confirmState)) {
        ui.emitSound(UiSoundEvent::Confirm);
        closeUiConfirmDialog(state);
        return UiConfirmDialogResult::Confirmed;
    }
    if (cancelRequested) {
        if (!cancelControlRequested) {
            ui.emitSound(UiSoundEvent::Cancel);
        }
        closeUiConfirmDialog(state);
        return UiConfirmDialogResult::Cancelled;
    }
    return UiConfirmDialogResult::None;
}

void drawUiConfirmDialogButtons(Renderer& renderer, const UiConfirmDialogState& state, UiRect panel)
{
    drawUiButton(
        renderer,
        uiConfirmDialogButtonRect(panel, 0),
        state.confirmLabel,
        state.selection == 0,
        state.confirmState,
        uiActionButtonStyle());
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

    UiModalNavigationScope navigationScope(panel);
    UiWindowScope window(renderer, id, panel, state.title, uiConfirmDialogHelpText(), UiWindowOptions{true, true});
    drawUiWindowBodyText(
        renderer,
        panel,
        state.title,
        state.message,
        uiConfirmDialogButtonRect(panel, 0).pos.y,
        ui::Text,
        UiConfirmDialogTextScale);
    drawUiConfirmDialogButtons(renderer, state, panel);
}

void openUiQuantityDialog(
    UiQuantityDialogState& state,
    std::string title,
    int minValue,
    int maxValue,
    int initialValue,
    std::string unitLabel)
{
    state.open = true;
    state.title = std::move(title);
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

    if (input.pressed(InputAction::MoveUp)) {
        adjust(1);
    }
    if (input.pressed(InputAction::MoveDown)) {
        adjust(-1);
    }
    if (input.pressed(InputAction::MoveRight)) {
        adjust(state.largeStep);
    }
    if (input.pressed(InputAction::MoveLeft)) {
        adjust(-state.largeStep);
    }
    if (updateUiArrowButton(
            ui,
            uiQuantityDownButtonRect(panel),
            UiArrowDirection::Down,
            UiArrowButtonVariant::Wide,
            state.value > state.minValue)) {
        adjust(-1);
    }
    if (updateUiArrowButton(
            ui,
            uiQuantityUpButtonRect(panel),
            UiArrowDirection::Up,
            UiArrowButtonVariant::Wide,
            state.value < state.maxValue)) {
        adjust(1);
    }
    ui.emitCursorMoveIfChanged(previousValue, state.value);
    if (ui.pressed(uiQuantityConfirmButtonRect(panel)) ||
        input.confirmPressed() ||
        input.useItemPressed()) {
        ui.emitSound(UiSoundEvent::Confirm);
        closeUiQuantityDialog(state);
        return UiQuantityDialogResult::Confirmed;
    }
    if (uiCancelControlRequested(input, ui, panel)) {
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

    UiExclusiveNavigationScope navigationScope(panel);
    const std::string helpText = buildInputHelpText({
        {InputHelpGroup::Primary, {InputAction::Confirm, InputAction::UseSelectedItem}, "決定"},
        {InputHelpGroup::Back, {InputAction::Cancel, InputAction::Pause}, "キャンセル"},
        {InputHelpGroup::Other, {InputAction::MoveUp, InputAction::MoveDown}, "1個ずつ"},
        {InputHelpGroup::Other, {InputAction::MoveLeft, InputAction::MoveRight}, "10個ずつ"},
    });
    UiWindowScope window(
        renderer,
        id,
        panel,
        state.title,
        helpText,
        UiWindowOptions{true, true});
    const UiRect controlRect = uiQuantityControlRect(panel);
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
            controlRect.pos.x + (controlRect.size.x - valueSize.x) * 0.5f,
            textBandTop + std::max(0.0f, (textBandBottom - textBandTop - valueSize.y) * 0.5f),
        },
        valueText,
        ui::Text,
        4);

    drawUiArrowButton(
        renderer,
        upButton,
        UiArrowDirection::Up,
        UiArrowButtonVariant::Wide,
        state.value < state.maxValue);
    drawUiArrowButton(
        renderer,
        downButton,
        UiArrowDirection::Down,
        UiArrowButtonVariant::Wide,
        state.value > state.minValue);

    drawUiButton(renderer, uiQuantityConfirmButtonRect(panel), "決定", false, uiActionButtonStyle());
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
    if (backPressed) {
        ui.consumeBackInput();
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
    int pressedIndex = -1;
    for (int i = 0; i < itemCount; ++i) {
        const UiRect rect = commandMenuItemRect(state, i);
        if (ui.navigationFocused(rect)) {
            state.hoveredIndex = i;
        }
        if (ui.hovered(rect)) {
            state.hoveredIndex = i;
            hoveredByMouse = true;
        }
        if (ui.pressed(rect)) {
            pressedIndex = i;
        }
    }
    if (!hoveredByMouse && (state.hoveredIndex < 0 || state.hoveredIndex >= itemCount)) {
        state.hoveredIndex = 0;
    }
    ui.emitCursorMoveIfChanged(previousHoveredIndex, state.hoveredIndex);

    if (pressedIndex >= 0) {
        if (!tryActivateUiButton(ui, items[pressedIndex].state)) {
            return -1;
        }
        const int selected = pressedIndex;
        ui.emitSound(UiSoundEvent::Confirm);
        closeUiCommandMenu(state);
        return selected;
    }

    if (!input.mouseLeftPressed() || ui.pointerConsumed()) {
        return -1;
    }

    const bool insidePanel = state.panel.contains(ui.mouse());
    if (!insidePanel) {
        ui.consumePointer();
        ui.emitSound(UiSoundEvent::Cancel);
        closeUiCommandMenu(state);
        return -1;
    }

    return -1;
}

void drawUiCommandMenu(Renderer& renderer, const UiCommandMenuState& state, const UiCommandMenuItem* items, int itemCount)
{
    if (!state.visible || items == nullptr || itemCount <= 0) {
        return;
    }

    UiModalNavigationScope navigationScope(state.panel);
    const float t = easeOut(state.animation);
    const Vec2 center = panelCenter(state.panel);
    renderer.pushScreenTransform(center, lerp(0.92f, 1.0f, t), t);
    drawUiSubPanel(renderer, state.panel);
    const float elapsed = static_cast<float>(SDL_GetTicks()) / 1000.0f;
    const float pulse = 0.5f + 0.5f * std::sin(elapsed * 6.283185307f);
    const unsigned char alpha = static_cast<unsigned char>(std::lround(96.0f + 140.0f * pulse));
    for (int i = 0; i < itemCount; ++i) {
        const UiRect rect = commandMenuItemRect(state, i);
        const bool preferred = i == state.hoveredIndex;
        const bool hot = uiControlVisualState(rect).selected;
        registerUiNavigationTarget(rect, UiNavigationRole::Control, preferred);
        UiControlMotionScope motion(renderer, rect, UiControlMotion::PressOnly);
        UiButtonStyle itemStyle;
        itemStyle.text = ui::Text;
        const Color text = uiButtonStyleForState(itemStyle, items[i].state).text;
        if (hot) {
            const UiRect cursorRect = commandMenuCursorRect(rect);
            if (renderer.hasUiRoundedRectangleTexture()) {
                renderer.drawUiRoundedRectangle(cursorRect.pos, cursorRect.size, {255, 255, 255, alpha});
            } else {
                fillRoundedRect(renderer, cursorRect, 8.0f, {48, 68, 138, alpha});
            }
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
            rememberUiControlVisualSelection(scrollAreaTrackRect(layout, style), true);
            scrollAreaSetThumbY(layout, style, ui.mouse().y - state->scrollbarDragOffsetY, scrollOffset);
            layout = makeUiScrollAreaLayout(viewport, contentHeight, scrollOffset, style);
            ui.consumePointer();
            return layout;
        }
        state->draggingScrollbar = false;
    }

    if (state != nullptr && layout.scrollable) {
        const UiRect track = scrollAreaTrackRect(layout, style);
        if (ui.pressed(track) && !ui.navigationActive()) {
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
    UiControlMotionScope motion(renderer, track, UiControlMotion::HoverAndPress);
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
            const UiRect cellRect = uiSelectableTableCellRect(layout, columns, columnCount, row, column, style);
            if (!columns[column].enabled) {
                if (ui.pressed(cellRect)) {
                    ui.rejectAction();
                }
                continue;
            }
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
        if (!state.open && count <= 0) {
            ui.rejectAction();
            return -1;
        }
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
                } else {
                    ui.rejectAction();
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
            ui.rejectAction();
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

    const int count = std::max(0, itemCount);
    const UiRect listRect = uiDropdownListRect(buttonRect, count, style);
    UiModalNavigationScope navigationScope(listRect);
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

int uiCycleInputDelta(const Input& input, int itemCount)
{
    return itemCount > 1 ? input.cycleDelta() : 0;
}

UiTabsInput makeUiCycleTabsInput(const Input& input, int itemCount)
{
    UiTabsInput tabsInput{};
    tabsInput.focusDelta = uiCycleInputDelta(input, itemCount);
    tabsInput.commit = tabsInput.focusDelta != 0;
    return tabsInput;
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
        state.hoveredIndex = -1;
        state.navigationFocused = false;
        return -1;
    }

    const int previousHoveredIndex = state.hoveredIndex;
    clampTabFocus(state, selectedIndex, items, itemCount);
    state.hoveredIndex = -1;
    state.navigationFocused = false;

    for (int i = 0; i < itemCount; ++i) {
        if (tabItemEnabled(items, i) && ui.navigationFocused(rects[i])) {
            state.focusedIndex = i;
            state.navigationFocused = true;
        }
        if (tabItemEnabled(items, i) && ui.hovered(rects[i])) {
            state.focusedIndex = i;
            state.hoveredIndex = i;
        }
        if (ui.pressed(rects[i])) {
            if (!tabItemEnabled(items, i)) {
                ui.rejectAction();
                return -1;
            }
            state.focusedIndex = i;
            if (i != selectedIndex) {
                ui.emitSound(UiSoundEvent::TabSwitch);
            }
            return i;
        }
    }

    if (state.hoveredIndex >= 0 && state.hoveredIndex != previousHoveredIndex) {
        ui.emitSound(UiSoundEvent::CursorMove);
    }

    if (input.directFocusIndex >= 0 && input.directFocusIndex < itemCount) {
        if (tabItemEnabled(items, input.directFocusIndex)) {
            state.focusedIndex = input.directFocusIndex;
        } else if (input.commit) {
            ui.rejectAction();
            return -1;
        }
    }
    if (input.focusDelta != 0) {
        moveTabFocus(state, input.focusDelta, items, itemCount, style);
    }

    if (input.commit && state.focusedIndex >= 0 && state.focusedIndex < itemCount &&
        state.focusedIndex != selectedIndex && tabItemEnabled(items, state.focusedIndex)) {
        // Z/X などの直接切替時、羽ペンがこのタブ群にある場合は選択と一緒に移す。
        // 旧タブのフォーカスが次フレームに選択を引き戻すことを防ぎつつ、
        // 別のコントロール上にある羽ペンは動かさない。
        if (ui.navigationActive() && state.navigationFocused) {
            ui.setNavigationFocus(rects[state.focusedIndex]);
        }
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

            const bool enabled = tabItemEnabled(items, i);
            const UiControlVisualState visual = uiControlVisualState(rects[i]);
            Color tint = selected ? buttonStyle.imageTintHot : buttonStyle.imageTint;
            if (visual.pressed && enabled) {
                tint = scaledColor(tint, UiHorizontalTabPressedBrightness);
            } else if (visual.selected && enabled) {
                tint = scaledColor(tint, UiHorizontalTabFocusBrightness);
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
            const UiControlVisualState visual = uiControlVisualState(rects[i]);
            if (selected) {
                buttonStyle.text = style.selectedText;
            }
            if (visual.pressed && enabled) {
                buttonStyle.text = scaledColor(buttonStyle.text, UiHorizontalTabPressedBrightness);
            } else if (visual.selected && enabled) {
                buttonStyle.text = scaledColor(buttonStyle.text, UiHorizontalTabFocusBrightness);
            }
            if (!enabled) {
                buttonStyle.text = ui::TextDisabled;
            }

            UiRect rect = rects[i];
            rect.size.y = ui::ButtonHeight;
            Color iconTint = selected ? style.selectedImageTint : Color{255, 255, 255, 255};
            if (visual.pressed && enabled) {
                iconTint = scaledColor(iconTint, UiHorizontalTabPressedBrightness);
            } else if (visual.selected && enabled) {
                iconTint = scaledColor(iconTint, UiHorizontalTabFocusBrightness);
            }
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
            if (visual.selected && enabled) {
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
        const UiControlVisualState visual = uiControlVisualState(rects[i]);
        const bool enabled = tabItemEnabled(items, i);
        if (visual.pressed && enabled) {
            buttonStyle.text = scaledColor(buttonStyle.text, UiHorizontalTabPressedBrightness);
        } else if (visual.selected && enabled) {
            buttonStyle.text = scaledColor(buttonStyle.text, UiHorizontalTabFocusBrightness);
        }

        const float imageOutset = std::max(0.0f, style.imageOutset);
        const UiRect imageRect{
            rect.pos - Vec2{imageOutset, imageOutset},
            rect.size + Vec2{imageOutset * 2.0f, imageOutset * 2.0f},
        };

        if (renderer.hasUiTabTexture()) {
            Color tint = selected ? buttonStyle.imageTintHot : buttonStyle.imageTint;
            if (visual.pressed) {
                tint = scaledColor(tint, UiHorizontalTabPressedBrightness);
            } else if (visual.selected) {
                tint = scaledColor(tint, UiHorizontalTabFocusBrightness);
            }
            renderer.drawUiTabFrame(imageRect.pos, imageRect.size, selected, tint);

            Color iconTint = selected ? style.selectedImageTint : Color{255, 255, 255, 255};
            if (visual.pressed && enabled) {
                iconTint = scaledColor(iconTint, UiHorizontalTabPressedBrightness);
            } else if (visual.selected && enabled) {
                iconTint = scaledColor(iconTint, UiHorizontalTabFocusBrightness);
            }
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
        } else {
            Color fill = selected ? buttonStyle.fillHot : buttonStyle.fill;
            Color outline = selected ? scaledColor(buttonStyle.outlineHot, 1.04f) : buttonStyle.outline;
            if (visual.pressed) {
                fill = scaledColor(fill, UiHorizontalTabPressedBrightness);
                outline = scaledColor(outline, UiHorizontalTabPressedBrightness);
            } else if (visual.selected) {
                fill = scaledColor(fill, UiHorizontalTabFocusBrightness);
                outline = scaledColor(outline, UiHorizontalTabFocusBrightness);
            }
            renderer.fillRect(imageRect.pos, imageRect.size, fill);
            renderer.drawRect(imageRect.pos, imageRect.size, outline);

            Color iconTint = selected ? style.selectedImageTint : Color{255, 255, 255, 255};
            if (visual.pressed && enabled) {
                iconTint = scaledColor(iconTint, UiHorizontalTabPressedBrightness);
            } else if (visual.selected && enabled) {
                iconTint = scaledColor(iconTint, UiHorizontalTabFocusBrightness);
            }
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
        }
        if (visual.selected) {
            requestUiSelectionCursor(rect);
        }
    };

    for (int i = 0; i < itemCount; ++i) {
        const bool active = i == selectedIndex ||
            (uiControlVisualState(rects[i]).selected && tabItemEnabled(items, i));
        if (!active) {
            drawTab(i);
        }
    }
    for (int i = 0; i < itemCount; ++i) {
        const bool active = i == selectedIndex ||
            (uiControlVisualState(rects[i]).selected && tabItemEnabled(items, i));
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
        const UiControlVisualState visual = uiControlVisualState(rects[i]);
        const bool focused = enabled && visual.selected;
        const UiRect rect = rects[i];

        if (selected) {
            Color fill = focused ? scaledColor(style.selectedFill, UiHorizontalTabFocusBrightness) : style.selectedFill;
            if (visual.pressed) {
                fill = scaledColor(fill, UiHorizontalTabPressedBrightness);
            }
            fillFadedTab(rect, fill);
        } else if (focused) {
            fillFadedTab(
                rect,
                visual.pressed
                    ? scaledColor(style.hoverFill, UiHorizontalTabPressedBrightness)
                    : style.hoverFill);
        }

        Color textColor = enabled
            ? (selected ? style.selectedText : style.text)
            : style.disabledText;
        if (enabled && visual.pressed) {
            textColor = scaledColor(textColor, UiHorizontalTabPressedBrightness);
        } else if (enabled && focused) {
            textColor = scaledColor(textColor, UiHorizontalTabFocusBrightness);
        }
        Color iconTint = selected ? style.selectedText : Color{255, 255, 255, 255};
        if (enabled && visual.pressed) {
            iconTint = scaledColor(iconTint, UiHorizontalTabPressedBrightness);
        } else if (enabled && focused) {
            iconTint = scaledColor(iconTint, UiHorizontalTabFocusBrightness);
        }
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
        if (focused) {
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
        const UiRect rect = rects[i];
        UiControlMotionScope motion(renderer, rect, UiControlMotion::HoverAndPress, enabled);

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
        if (uiControlVisualState(rect).selected && enabled) {
            requestUiSelectionCursor(rect);
        }
    };

    for (int i = 0; i < itemCount; ++i) {
        const bool active = i == selectedIndex ||
            (uiControlVisualState(rects[i]).selected && itemEnabled(i));
        if (!active) {
            drawTab(i);
        }
    }
    for (int i = 0; i < itemCount; ++i) {
        const bool active = i == selectedIndex ||
            (uiControlVisualState(rects[i]).selected && itemEnabled(i));
        if (active) {
            drawTab(i);
        }
    }
}

}
