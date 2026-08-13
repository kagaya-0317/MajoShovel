#pragma once

#include "engine/Input.hpp"
#include "engine/Math.hpp"
#include "engine/Renderer.hpp"

#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

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
inline constexpr bool SystemMessagesVisible = false;
inline constexpr Vec2 ButtonTextPadding{18.0f, 2.0f};
inline constexpr float ButtonHeight = 53.0f;
inline constexpr float HeaderHeight = 92.0f;
inline constexpr float FooterLineHeight = 24.0f;
inline constexpr float FooterPaddingY = 8.0f;
inline constexpr float FooterSingleLineHeight = FooterLineHeight + FooterPaddingY * 2.0f;
inline constexpr float FooterMaxHeight = FooterLineHeight * 2.0f + FooterPaddingY * 2.0f;
inline constexpr float FooterActionGap = 12.0f;
inline constexpr float FooterActionPairGap = 48.0f;
inline constexpr float PanelPadding = 24.0f;
inline constexpr float ButtonGap = 10.0f;
inline constexpr Vec2 HeaderTitlePadding{24.0f, 32.0f};
inline constexpr Vec2 FooterTextPadding{24.0f, 8.0f};
inline constexpr Vec2 FooterHelpTextOffset{12.0f, 0.0f};
inline constexpr Vec2 ImageWindowHeaderTitlePadding{48.0f, 40.0f};
inline constexpr Vec2 ImageWindowFooterTextPadding{48.0f, 0.0f};
inline constexpr Vec2 SubPanelPadding{24.0f, 24.0f};
inline constexpr Vec2 CancelButtonSize{58.0f, 60.0f};
inline constexpr Vec2 CancelButtonOffset{-6.0f, 4.0f};
inline constexpr Vec2 ArrowButtonSize{48.0f, 48.0f};
inline constexpr Vec2 ArrowButtonWideSize{144.0f, 32.0f};
inline constexpr float DecoratedWindowMinWidth = 561.0f;
inline constexpr float DecoratedWindowMinHeight = 223.0f;
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

struct UiChoiceWindowLayout {
    float choiceTopInset = 20.0f;
    float choiceHorizontalInset = 22.0f;
    float choiceGap = 16.0f;
    float footerGap = 32.0f;
};

inline UiRect uiEnsureDecoratedWindowMinSize(UiRect rect)
{
    const float widthExpansion = ui::DecoratedWindowMinWidth - rect.size.x;
    if (widthExpansion > 0.0f) {
        rect.pos.x -= widthExpansion * 0.5f;
        rect.size.x = ui::DecoratedWindowMinWidth;
    }
    const float heightExpansion = ui::DecoratedWindowMinHeight - rect.size.y;
    if (heightExpansion > 0.0f) {
        rect.pos.y -= heightExpansion * 0.5f;
        rect.size.y = ui::DecoratedWindowMinHeight;
    }
    return rect;
}

enum class UiSoundEvent {
    Confirm,
    Cancel,
    Error,
    MenuOpen,
    TabSwitch,
    BookOpen,
    CursorMove,
    ItemMove,
    ItemUse,
    Equip,
    RingPlace,
    UpgradeSelect,
    Count,
};

// 選択はできるが、現在の状態では実行できないボタンを Unavailable とする。
// ナビゲーション対象外にする一般的な disabled とは意味を分けて扱う。
enum class UiButtonState {
    Enabled,
    Unavailable,
};

constexpr UiButtonState uiButtonState(bool enabled)
{
    return enabled ? UiButtonState::Enabled : UiButtonState::Unavailable;
}

constexpr bool uiButtonAvailable(UiButtonState state)
{
    return state == UiButtonState::Enabled;
}

enum class UiNavigationRole {
    Control,
    Tab,
    Grid,
    Slider,
};

enum class UiControlMotion {
    HoverAndPress,
    PressOnly,
};

struct UiControlVisualState {
    bool selected = false;
    bool pressed = false;
};

class UiContext {
public:
    UiContext(const Input& input, Renderer& renderer);

    Vec2 mouse() const { return mouse_; }
    bool pointerConsumed() const { return pointerConsumed_; }
    bool backInputConsumed() const;
    // 生の座標判定。ドラッグなど、入力方式にかかわらず継続するポインター操作に使う。
    bool pointerInside(UiRect rect) const;
    // マウスが現在の入力方式である間だけ有効になるホバー判定。
    bool hovered(UiRect rect) const;
    bool pressed(UiRect rect);
    // 動く見た目を持つコントロール向け。hitRect は現在のポインター判定、
    // controlRect は安定したフォーカス識別・押下アニメーションに使う。
    bool pressed(UiRect hitRect, UiRect controlRect);
    bool pressedImage(
        UiRect rect,
        std::string_view imagePath,
        const ImageDrawOptions& options = {},
        unsigned char alphaThreshold = 1,
        TextureFilter filter = TextureFilter::Nearest);
    bool navigationActive() const;
    bool navigationFocused(UiRect rect) const;
    void setNavigationFocus(UiRect rect);
    // マウスホバーとキーボード／ゲームパッドフォーカスを排他的に統合する。
    bool selectionFocused(UiRect rect) const;
    bool selectionFocused(UiRect hitRect, UiRect controlRect) const;
    UiNavigationRole navigationFocusRole() const;
    void consumePointer() { pointerConsumed_ = true; }
    void consumeBackInput();
    void block(UiRect rect);
    void emitSound(UiSoundEvent event);
    void emitActionResult(bool succeeded, UiSoundEvent successEvent = UiSoundEvent::Confirm);
    void rejectAction();
    void emitCursorMoveIfChanged(int previousIndex, int currentIndex);
    int soundEventCount(UiSoundEvent event) const;
    bool hasSoundEvents() const;

private:
    bool pressedWithPointerHit(UiRect rect, bool pointerHit);

    Renderer& renderer_;
    Vec2 mouse_{};
    bool mouseLeftPressed_ = false;
    bool mouseLeftHeld_ = false;
    bool pointerActive_ = false;
    bool navigationConfirmPressed_ = false;
    bool navigationConfirmHeld_ = false;
    bool navigationConfirmConsumed_ = false;
    bool pointerConsumed_ = false;
    int soundEventCounts_[static_cast<int>(UiSoundEvent::Count)]{};
};

struct UiButtonStyle {
    Color fill{30, 46, 104, 218};
    Color fillHot{40, 60, 126, 234};
    Color outline{255, 255, 255, 255};
    Color outlineHot{255, 255, 255, 255};
    Color text{255, 255, 255, 255};
    int imageVariant = 0;
    Color imageTint{232, 232, 238, 255};
    Color imageTintHot{255, 255, 235, 255};
};

struct UiSystemMessageStyle {
    Color text{255, 230, 150, 255};
    Color fill{0, 0, 0, 0};
    Vec2 padding{0.0f, 0.0f};
    float maxWidth = 0.0f;
    int textScale = 2;
};

struct UiSmallSelectButtonStyle {
    Color fill{24, 36, 78, 196};
    Color fillHot{52, 70, 128, 228};
    Color fillDisabled{18, 24, 42, 170};
    Color outline{118, 104, 136, 220};
    Color outlineHot{255, 230, 150, 255};
    Color outlineDisabled{90, 84, 108, 170};
    Color text{255, 255, 255, 255};
    Color valueText{198, 198, 206, 255};
    Color disabledText{150, 150, 160, 255};
    Color accent{255, 230, 150, 255};
    int textScale = 3;
    int valueTextScale = 2;
};

struct UiTextInputState {
    std::string text;
    std::string composition;
    bool focused = false;
    bool textInputActive = false;
};

struct UiTextInputStyle {
    Color fill{18, 24, 48, 214};
    Color fillFocused{26, 38, 78, 232};
    Color outline{112, 128, 178, 190};
    Color outlineFocused{255, 230, 150, 255};
    Color text{255, 255, 255, 255};
    Color placeholder{172, 178, 198, 220};
    Color caret{255, 246, 190, 255};
    Vec2 padding{14.0f, 0.0f};
    int textScale = 2;
};

struct UiTextInputResult {
    bool focusedChanged = false;
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
    Color highlight{255, 255, 255, 96};
    Color shimmer{255, 255, 255, 0};
    Color outline{180, 167, 127, 255};
    Color outerOutline{0, 0, 0, 255};
    float cornerRadius = -1.0f;
    float outerOutlineWidth = 2.0f;
    float trackOuterExtra = 2.0f;
    float trackInnerInset = 5.0f;
    float shadowOffsetY = 3.0f;
    float shadowExtra = 8.0f;
    float shimmerPhase = -1.0f;
    float shimmerWidth = 56.0f;
};

struct UiColoredTextRun {
    std::string_view text;
    Color color = ui::Text;
};

struct UiWrappedColoredTextStyle {
    int scale = 2;
    std::string_view hangingIndentText;
};

struct UiDetailTextStyle {
    Color color = ui::Text;
    int scale = 2;
    float bottomGap = 8.0f;
};

struct UiDetailLineStyle {
    Color labelColor = ui::TextMuted;
    Color valueColor = ui::Text;
    int scale = 2;
    float labelWidth = 106.0f;
    float minLineHeight = 31.0f;
    float lineGap = 4.0f;
};

enum class UiArrowDirection {
    Up,
    Right,
    Down,
    Left,
};

enum class UiArrowButtonVariant {
    Standard,
    Wide,
};

struct UiSliderSpec {
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float step = 0.0f;
    bool showReference = false;
    float referenceValue = 0.0f;
    int valueDecimalPlaces = 0;
    std::string_view valueSuffix;
};

struct UiSliderStyle {
    Color track{76, 84, 104, 210};
    Color activeTrack{132, 230, 250, 255};
    Color thumb{132, 230, 250, 255};
    Color thumbOutline{255, 255, 255, 96};
    Color valueBubble{54, 58, 68, 245};
    Color valueText{255, 255, 255, 255};
    float trackThickness = 4.0f;
    float thumbRadius = -1.0f;
    bool showValueBubble = true;
    bool navigationEnabled = false;
};

enum class UiSliderInteractionState : unsigned char {
    Idle,
    ValueVisible,
    Dragging,
};

struct UiSliderState {
    UiSliderInteractionState interaction = UiSliderInteractionState::Idle;

    [[nodiscard]] bool dragging() const { return interaction == UiSliderInteractionState::Dragging; }
    [[nodiscard]] bool valueBubbleVisible() const { return interaction != UiSliderInteractionState::Idle; }
    void beginDrag() { interaction = UiSliderInteractionState::Dragging; }
    void releaseDrag() { interaction = UiSliderInteractionState::ValueVisible; }
    void showValue() { interaction = UiSliderInteractionState::ValueVisible; }
    void dismissValue() { interaction = UiSliderInteractionState::Idle; }
};

static_assert(sizeof(UiSliderState) == sizeof(bool));

struct UiSliderResult {
    float value = 0.0f;
    bool changed = false;
    bool interacting = false;
};

struct UiCommandMenuItem {
    std::string_view label{};
    UiButtonState state = UiButtonState::Enabled;

    constexpr UiCommandMenuItem() = default;
    constexpr UiCommandMenuItem(std::string_view itemLabel, UiButtonState itemState = UiButtonState::Enabled)
        : label(itemLabel)
        , state(itemState)
    {
    }
    constexpr UiCommandMenuItem(std::string_view itemLabel, bool enabled)
        : UiCommandMenuItem(itemLabel, uiButtonState(enabled))
    {
    }
};

struct UiCommandMenuState {
    bool open = false;
    bool visible = false;
    bool closing = false;
    bool openSoundPending = false;
    UiRect panel{};
    int hoveredIndex = -1;
    int textScale = 2;
    float animation = 0.0f;
};

struct UiResultDialogSegment {
    std::string text;
    Color color{ui::Text};
};

struct UiResultDialogLine {
    std::vector<UiResultDialogSegment> segments;
};

struct UiResultDialogState {
    bool open = false;
    std::string title;
    std::vector<UiResultDialogLine> lines;
};

struct UiConfirmDialogState {
    bool open = false;
    std::string title;
    std::string message;
    std::string confirmLabel = "はい";
    std::string cancelLabel = "いいえ";
    int selection = 1;
    UiButtonState confirmState = UiButtonState::Enabled;
};

enum class UiConfirmDialogResult {
    None,
    Confirmed,
    Cancelled,
};

struct UiQuantityDialogState {
    bool open = false;
    std::string title;
    std::string unitLabel;
    int value = 1;
    int minValue = 1;
    int maxValue = 1;
    int largeStep = 10;
};

enum class UiQuantityDialogResult {
    None,
    Confirmed,
    Cancelled,
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
    std::string_view emptyLabel = "項目がないよ";
};

struct UiScrollAreaStyle {
    float wheelStep = 48.0f;
    float scrollbarWidth = 8.0f;
    float scrollbarGap = 6.0f;
    float scrollbarPaddingX = 6.0f;
    float scrollbarPaddingY = 6.0f;
    float scrollbarMinThumbHeight = 24.0f;
    Color scrollbarTrack{255, 255, 255, 48};
    Color scrollbarThumb{255, 255, 255, 170};
    Color outline{255, 255, 255, 170};
};

struct UiScrollAreaState {
    bool draggingScrollbar = false;
    float scrollbarDragOffsetY = 0.0f;
};

struct UiScrollAreaLayout {
    UiRect viewport{};
    UiRect content{};
    float contentHeight = 0.0f;
    float scrollOffset = 0.0f;
    float maxScroll = 0.0f;
    float scrollbarReserve = 0.0f;
    bool scrollable = false;
};

struct UiScrollableListStyle {
    float rowHeight = 44.0f;
    float rowGap = 4.0f;
    float leadingPadding = 0.0f;
    float trailingPadding = 0.0f;
    float rowInsetX = 0.0f;
    UiScrollAreaStyle scroll{};
};

struct UiSelectableTableColumn {
    std::string_view label;
    float width = 0.0f;
    bool enabled = true;
};

struct UiSelectableTableState {
    int selectedRow = 0;
    int selectedColumn = 0;
    float scrollOffset = 0.0f;
    UiScrollAreaState scroll{};
};

struct UiSelectableTableStyle {
    float headerHeight = 34.0f;
    float rowHeight = 44.0f;
    float rowGap = 4.0f;
    float columnGap = 8.0f;
    float cellPaddingX = 12.0f;
    int headerTextScale = 2;
    int cellTextScale = 2;
    Color headerFill{18, 24, 48, 214};
    Color rowFill{20, 30, 68, 190};
    Color rowFillHot{42, 58, 118, 224};
    Color cellOutline{112, 128, 178, 160};
    Color cellOutlineHot{255, 230, 150, 255};
    Color headerText{220, 226, 244, 255};
    Color text{255, 255, 255, 255};
    Color disabledText{150, 150, 160, 255};
    UiScrollAreaStyle scroll{};
};

struct UiSelectableTableLayout {
    UiRect header{};
    UiScrollAreaLayout scroll{};
};

struct UiSelectableTableResult {
    int pressedRow = -1;
    int pressedColumn = -1;
    bool selectionChanged = false;
};

struct UiTabItem {
    std::string_view label;
    bool enabled = true;
    int iconImageNumber = 0;
};

struct UiVerticalTabItem {
    std::string_view label;
    std::string_view value;
    bool enabled = true;
    Color valueText = ui::TextMuted;
};

struct UiTabsInput {
    int focusDelta = 0;
    int directFocusIndex = -1;
    bool commit = false;
};

struct UiTabsState {
    int focusedIndex = -1;
    int hoveredIndex = -1;
    bool navigationFocused = false;
};

struct UiTabsStyle {
    UiButtonStyle buttonStyle{};
    Color selectedFillHot{62, 84, 166, 244};
    Color selectedOutlineHot{255, 246, 190, 255};
    Color selectedText{255, 250, 224, 255};
    Color selectedImageTint{255, 255, 255, 255};
    Color focusOutline{255, 228, 138, 210};
    float visualGap = 8.0f;
    float imageOutset = 18.0f;
    float activeScale = 1.035f;
    bool wrapKeyboard = true;
};

struct UiSubTabsStyle {
    Color barFill{0, 0, 0, 118};
    Color selectedFill{92, 218, 246, 218};
    Color hoverFill{255, 255, 255, 82};
    Color text{255, 255, 255, 255};
    Color selectedText{12, 30, 50, 255};
    Color disabledText = ui::TextDisabled;
    float fadeWidth = 28.0f;
    float sidePadding = 28.0f;
    float tabFadeWidth = 30.0f;
    float textOffsetY = 2.0f;
    int textScale = 2;
    bool wrapKeyboard = true;
};

struct UiVerticalTabsStyle {
    UiTabsStyle tabs{};
    float textPaddingX = 18.0f;
    float valuePaddingX = 20.0f;
    float valueGap = 10.0f;
    float textOffsetY = 2.0f;
    int labelScale = 3;
    int valueScale = 2;
    Color disabledText = ui::TextDisabled;
};

enum class UiWindowFrame {
    Default,
    Message,
    SystemMessage,
};

struct UiWindowOptions {
    bool animated = true;
    bool cancelButton = false;
    UiWindowFrame frame = UiWindowFrame::Default;
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

class UiControlMotionScope {
public:
    UiControlMotionScope(
        Renderer& renderer,
        UiRect rect,
        UiControlMotion motion = UiControlMotion::HoverAndPress,
        bool enabled = true);
    UiControlMotionScope(
        Renderer& renderer,
        UiRect visualRect,
        UiRect controlRect,
        UiControlMotion motion,
        bool enabled = true);
    ~UiControlMotionScope();
    UiControlMotionScope(const UiControlMotionScope&) = delete;
    UiControlMotionScope& operator=(const UiControlMotionScope&) = delete;

private:
    Renderer* renderer_ = nullptr;
    bool transformed_ = false;
};

// 入力更新で記録された、そのフレームの排他的な選択／押下状態。
// 永続的な selectedIndex などはナビゲーションの復帰位置にだけ使い、
// 描画上の強調はこの状態を基準にする。
[[nodiscard]] UiControlVisualState uiControlVisualState(UiRect rect);

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

void beginUiFrame(float dt, bool navigationCursorEnabled = false);
void finishUiFrame(Renderer& renderer);

class UiNavigationLayerScope {
public:
    UiNavigationLayerScope();
    ~UiNavigationLayerScope();

    UiNavigationLayerScope(const UiNavigationLayerScope&) = delete;
    UiNavigationLayerScope& operator=(const UiNavigationLayerScope&) = delete;

private:
    int previousLayer_ = 0;
};

// 背面のナビゲーション対象と混ざらない、モーダル専用レイヤーを作る。
// 実際の操作対象が一つもない場合も、無効な番兵で背面へのフォーカス漏れを防ぐ。
class UiModalNavigationScope {
public:
    explicit UiModalNavigationScope(UiRect modalRect);
    ~UiModalNavigationScope();

    UiModalNavigationScope(const UiModalNavigationScope&) = delete;
    UiModalNavigationScope& operator=(const UiModalNavigationScope&) = delete;

private:
    int previousLayer_ = 0;
};

// 方向入力そのものを数値変更やスクロールに使うモーダル向け。
// 背面を遮断したうえで、羽ペンの表示とフォーカス移動を止める。
class UiExclusiveNavigationScope {
public:
    explicit UiExclusiveNavigationScope(UiRect modalRect);
    ~UiExclusiveNavigationScope();

    UiExclusiveNavigationScope(const UiExclusiveNavigationScope&) = delete;
    UiExclusiveNavigationScope& operator=(const UiExclusiveNavigationScope&) = delete;

private:
    int previousLayer_ = 0;
    bool previousRegistrationEnabled_ = true;
};

void registerUiNavigationTarget(
    UiRect rect,
    UiNavigationRole role = UiNavigationRole::Control,
    bool preferred = false,
    bool enabled = true);
void requestUiSelectionCursor(UiRect rect);
void suppressUiSelectionCursor();
int moveUiGridSelection(int selectedIndex, int itemCount, int columns, int dx, int dy);

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
UiRect uiBodyRect(
    UiRect panel,
    float bottomExtension = 0.0f,
    float topExtension = 0.0f);
UiRect uiChoiceWindowRect(
    Vec2 position,
    float width,
    int choiceCount,
    std::string_view helpText,
    const UiChoiceWindowLayout& layout = {});
UiRect uiChoiceWindowButtonRect(
    UiRect panel,
    int index,
    const UiChoiceWindowLayout& layout = {});
Vec2 uiSubPanelContentPos(UiRect panel);
UiRect uiSubPanelContentRect(UiRect panel);
enum class UiFooterActionAlignment {
    Left,
    Center,
    Right,
};
UiRect uiFooterActionRowRect(UiRect panel);
UiRect uiFooterActionButtonRect(UiRect panel, Vec2 size, UiFooterActionAlignment alignment);
UiRect uiFooterActionGroupButtonRect(
    UiRect panel,
    Vec2 size,
    int index,
    int count,
    float gap = ui::ButtonGap);
UiRect uiBottomLeftButtonRect(UiRect panel, Vec2 size, float bodyBottomExtension = 0.0f);
UiRect uiBottomCenterButtonRect(UiRect panel, Vec2 size, float bodyBottomExtension = 0.0f);
UiRect uiBottomRightButtonRect(UiRect panel, Vec2 size, float bodyBottomExtension = 0.0f);
UiRect uiCancelButtonRect(UiRect panel);
bool uiCancelControlRequested(const Input& input, UiContext& ui, UiRect panel);
bool uiCancelRequested(UiCancelControlState& state, const Input& input, UiContext& ui, UiRect panel);

void setUiMenuIconScaleOverrides(const std::unordered_map<std::string, float>* scaleByIconKey);
void drawUiTextWithIcon(
    Renderer& renderer,
    Vec2 pos,
    std::string_view text,
    int iconImageNumber,
    Color textColor = ui::Text,
    int textScale = 2,
    float iconSize = 30.0f,
    float iconTextGap = 8.0f,
    Color iconTint = {255, 255, 255, 255});

void drawUiPanel(Renderer& renderer, UiRect panel, UiWindowFrame frame = UiWindowFrame::Default);
void drawUiSubPanel(Renderer& renderer, UiRect panel);
void drawUiHeader(Renderer& renderer, UiRect panel, std::string_view title, UiWindowFrame frame = UiWindowFrame::Default);
void drawUiFooter(Renderer& renderer, UiRect panel, std::string_view helpText, UiWindowFrame frame = UiWindowFrame::Default);
void drawUiBottomInputHelp(
    Renderer& renderer,
    UiRect safeArea,
    std::string helpText,
    float horizontalInset = 16.0f,
    float bottomInset = 4.0f);
void drawUiWindow(Renderer& renderer, UiRect panel, std::string_view title, std::string_view helpText = {});
void drawUiModalBackdrop(Renderer& renderer, UiRect bounds, Color color = {0, 0, 0, 150});
void drawUiCancelButton(Renderer& renderer, UiRect panel);
void drawUiSeparator(Renderer& renderer, UiRect rect, Color tint = {255, 255, 255, 255});
void drawUiGauge(Renderer& renderer, UiRect rect, float progress, const UiGaugeStyle& style = {});
UiSliderResult updateUiSlider(
    UiContext& ui,
    const Input& input,
    UiRect rect,
    float value,
    const UiSliderSpec& spec,
    UiSliderState& state,
    const UiSliderStyle& style = {});
void drawUiSlider(
    Renderer& renderer,
    UiRect rect,
    float value,
    const UiSliderSpec& spec,
    const UiSliderState& state,
    const UiSliderStyle& style = {});
bool tryActivateUiButton(UiContext& ui, UiButtonState state);
UiButtonStyle uiButtonStyleForState(UiButtonStyle style, UiButtonState state);
// preferred は、現在のフォーカス矩形が無効になったときに選ぶ初期候補。
// 実際のフォーカス表示は UiContext が管理するナビゲーション状態から描画する。
void drawUiButton(Renderer& renderer, UiRect rect, std::string_view label, bool preferred, const UiButtonStyle& style = {});
void drawUiButton(
    Renderer& renderer,
    UiRect rect,
    std::string_view label,
    bool preferred,
    UiButtonState state,
    const UiButtonStyle& style = {});
void drawUiButton(
    Renderer& renderer,
    UiRect rect,
    std::string_view label,
    bool preferred,
    const UiButtonStyle& style,
    UiNavigationRole navigationRole);
void drawUiButton(
    Renderer& renderer,
    UiRect rect,
    std::string_view label,
    bool preferred,
    UiButtonState state,
    const UiButtonStyle& style,
    UiNavigationRole navigationRole);
void drawUiButton(Renderer& renderer, UiRect rect, std::string_view label, int iconImageNumber, bool preferred, const UiButtonStyle& style = {});
void drawUiButton(
    Renderer& renderer,
    UiRect rect,
    std::string_view label,
    int iconImageNumber,
    bool preferred,
    UiButtonState state,
    const UiButtonStyle& style = {});
void drawUiFlexibleButtonFrame(Renderer& renderer, UiRect rect, bool preferred, const UiButtonStyle& style = {});
void drawUiFlexibleButtonFrame(
    Renderer& renderer,
    UiRect rect,
    bool preferred,
    UiButtonState state,
    const UiButtonStyle& style = {});
// Draws the flexible frame with a caller-owned persistent selection style.
// No motion transform is applied so the caller can wrap the complete control content in UiControlMotionScope.
void drawUiFlexibleButtonFrameWithVisualSelection(
    Renderer& renderer,
    UiRect rect,
    bool preferred,
    bool visuallySelected,
    const UiButtonStyle& style = {});
void drawUiFlexibleButton(Renderer& renderer, UiRect rect, std::string_view label, bool preferred, const UiButtonStyle& style = {});
void drawUiFlexibleButton(
    Renderer& renderer,
    UiRect rect,
    std::string_view label,
    bool preferred,
    UiButtonState state,
    const UiButtonStyle& style = {});
void drawUiRectButton(Renderer& renderer, UiRect rect, std::string_view label, bool hot, const UiButtonStyle& style = {});
void drawUiRectButton(
    Renderer& renderer,
    UiRect rect,
    std::string_view label,
    bool hot,
    UiButtonState state,
    const UiButtonStyle& style = {});
Vec2 uiArrowButtonNativeSize(UiArrowButtonVariant variant);
bool updateUiArrowButton(
    UiContext& ui,
    UiRect rect,
    UiArrowDirection direction,
    UiArrowButtonVariant variant,
    bool enabled = true);
void drawUiArrowButton(
    Renderer& renderer,
    UiRect rect,
    UiArrowDirection direction,
    UiArrowButtonVariant variant,
    bool enabled = true);
void drawUiSmallSelectButton(
    Renderer& renderer,
    UiRect rect,
    std::string_view label,
    std::string_view value,
    bool hot,
    bool disabled = false,
    const UiSmallSelectButtonStyle& style = {});
void focusUiTextInput(UiTextInputState& state);
void blurUiTextInput(UiTextInputState& state);
bool handleUiTextInputEvent(UiTextInputState& state, const SDL_Event& event, int maxCodepoints = 64);
UiTextInputResult updateUiTextInput(UiTextInputState& state, UiContext& ui, UiRect rect);
void drawUiTextInput(
    Renderer& renderer,
    UiRect rect,
    const UiTextInputState& state,
    std::string_view placeholder,
    const UiTextInputStyle& style = {});
void drawUiBodyMessageBelow(Renderer& renderer, UiRect anchor, std::string_view message, Color color = ui::TextMuted);
UiRect uiWindowBodyTextRect(
    Renderer& renderer,
    UiRect panel,
    std::string_view title,
    std::string_view text,
    float nextControlTop,
    int textScale = 2,
    UiWindowFrame frame = UiWindowFrame::Default);
void drawUiWindowBodyText(
    Renderer& renderer,
    UiRect panel,
    std::string_view title,
    std::string_view text,
    float nextControlTop,
    Color color = ui::TextMuted,
    int textScale = 2,
    UiWindowFrame frame = UiWindowFrame::Default);
void drawUiSystemMessage(Renderer& renderer, std::string_view message, Vec2 pos, const UiSystemMessageStyle& style = {});
[[nodiscard]] float measureUiWrappedColoredText(
    Renderer& renderer,
    std::span<const UiColoredTextRun> runs,
    float maxWidth,
    const UiWrappedColoredTextStyle& style = {});
[[nodiscard]] float drawUiWrappedColoredText(
    Renderer& renderer,
    Vec2 pos,
    std::span<const UiColoredTextRun> runs,
    float maxWidth,
    int scale = 2);
[[nodiscard]] float drawUiWrappedColoredText(
    Renderer& renderer,
    Vec2 pos,
    std::span<const UiColoredTextRun> runs,
    float maxWidth,
    const UiWrappedColoredTextStyle& style);
float drawUiDetailHeader(Renderer& renderer, UiRect panel, std::string_view text);
float drawUiDetailHeaderWithIcon(
    Renderer& renderer,
    UiRect panel,
    std::string_view text,
    int iconImageNumber);
void drawUiDetailText(Renderer& renderer, UiRect panel, float& y, std::string_view text);
void drawUiDetailText(Renderer& renderer, UiRect panel, float& y, std::string_view text, Color color);
void drawUiDetailText(Renderer& renderer, UiRect panel, float& y, std::string_view text, const UiDetailTextStyle& style);
void drawUiDetailLine(Renderer& renderer, UiRect panel, float& y, std::string_view label, std::string_view value, Color valueColor = ui::Text);
void drawUiDetailLine(
    Renderer& renderer,
    UiRect panel,
    float& y,
    std::string_view label,
    std::string_view value,
    const UiDetailLineStyle& style);
void openUiResultDialog(UiResultDialogState& state, std::string title, std::vector<std::string> lines);
void openUiResultDialog(UiResultDialogState& state, std::string title, std::vector<UiResultDialogLine> lines);
bool updateUiResultDialog(UiResultDialogState& state, UiContext& ui, const Input& input, UiRect panel);
void drawUiResultDialog(Renderer& renderer, const UiResultDialogState& state, UiRect panel, std::string_view id);
UiRect fitUiResultDialogRect(const UiResultDialogState& state, UiRect basePanel);
UiRect uiResultDialogOkButtonRect(UiRect panel);
void openUiConfirmDialog(
    UiConfirmDialogState& state,
    std::string title,
    std::string message,
    std::string confirmLabel = "はい",
    std::string cancelLabel = "いいえ",
    int defaultSelection = 1);
UiConfirmDialogResult updateUiConfirmDialog(UiConfirmDialogState& state, UiContext& ui, const Input& input, UiRect panel);
void drawUiConfirmDialog(Renderer& renderer, const UiConfirmDialogState& state, UiRect panel, std::string_view id);
std::string uiConfirmDialogHelpText();
void drawUiConfirmDialogButtons(Renderer& renderer, const UiConfirmDialogState& state, UiRect panel);
UiRect uiConfirmDialogButtonRect(UiRect panel, int index);
void openUiQuantityDialog(
    UiQuantityDialogState& state,
    std::string title,
    int minValue,
    int maxValue,
    int initialValue,
    std::string unitLabel = {});
UiQuantityDialogResult updateUiQuantityDialog(UiQuantityDialogState& state, UiContext& ui, const Input& input, UiRect panel);
void drawUiQuantityDialog(Renderer& renderer, const UiQuantityDialogState& state, UiRect panel, std::string_view id);
void openUiCommandMenu(
    UiCommandMenuState& state,
    Vec2 anchor,
    UiRect bounds,
    int itemCount,
    const UiCommandMenuItem* items = nullptr,
    float minWidth = 120.0f,
    int textScale = 2);
Vec2 uiCommandMenuAnchorForSlot(UiRect slotRect);
void closeUiCommandMenu(UiCommandMenuState& state);
int updateUiCommandMenu(UiCommandMenuState& state, UiContext& ui, const Input& input, const UiCommandMenuItem* items, int itemCount);
void drawUiCommandMenu(Renderer& renderer, const UiCommandMenuState& state, const UiCommandMenuItem* items, int itemCount);
UiScrollAreaLayout makeUiScrollAreaLayout(UiRect viewport, float contentHeight, float scrollOffset, const UiScrollAreaStyle& style = {});
UiScrollAreaLayout updateUiScrollArea(
    UiContext& ui,
    const Input& input,
    UiRect viewport,
    float contentHeight,
    float& scrollOffset,
    const UiScrollAreaStyle& style = {},
    UiScrollAreaState* state = nullptr);
bool uiScrollAreaRectVisible(const UiScrollAreaLayout& layout, UiRect rect);
void keepUiScrollAreaRectVisible(UiRect viewport, UiRect rect, float contentHeight, float& scrollOffset, const UiScrollAreaStyle& style = {});
void drawUiScrollAreaFrame(Renderer& renderer, const UiScrollAreaLayout& layout, const UiScrollAreaStyle& style = {});
void drawUiScrollAreaScrollbar(Renderer& renderer, const UiScrollAreaLayout& layout, const UiScrollAreaStyle& style = {});
float uiScrollableListContentHeight(int itemCount, const UiScrollableListStyle& style = {});
UiScrollAreaLayout makeUiScrollableListLayout(UiRect viewport, int itemCount, float scrollOffset, const UiScrollableListStyle& style = {});
UiScrollAreaLayout updateUiScrollableList(
    UiContext& ui,
    const Input& input,
    UiRect viewport,
    int itemCount,
    float& scrollOffset,
    const UiScrollableListStyle& style = {},
    UiScrollAreaState* state = nullptr);
UiRect uiScrollableListItemRect(const UiScrollAreaLayout& layout, int index, const UiScrollableListStyle& style = {});
void keepUiScrollableListItemVisible(UiRect viewport, int selectedIndex, int itemCount, float& scrollOffset, const UiScrollableListStyle& style = {});
float uiSelectableTableContentHeight(int rowCount, const UiSelectableTableStyle& style = {});
UiSelectableTableLayout makeUiSelectableTableLayout(
    UiRect rect,
    int rowCount,
    float scrollOffset,
    const UiSelectableTableStyle& style = {});
UiSelectableTableResult updateUiSelectableTable(
    UiSelectableTableState& state,
    UiContext& ui,
    const Input& input,
    UiRect rect,
    int rowCount,
    const UiSelectableTableColumn* columns,
    int columnCount,
    const UiSelectableTableStyle& style = {});
UiRect uiSelectableTableRowRect(const UiSelectableTableLayout& layout, int row, const UiSelectableTableStyle& style = {});
UiRect uiSelectableTableCellRect(
    const UiSelectableTableLayout& layout,
    const UiSelectableTableColumn* columns,
    int columnCount,
    int row,
    int column,
    const UiSelectableTableStyle& style = {});
void keepUiSelectableTableCellVisible(
    UiRect rect,
    int row,
    int rowCount,
    float& scrollOffset,
    const UiSelectableTableStyle& style = {});
void drawUiSelectableTableFrame(
    Renderer& renderer,
    const UiSelectableTableLayout& layout,
    const UiSelectableTableColumn* columns,
    int columnCount,
    const UiSelectableTableStyle& style = {});
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
int uiCycleInputDelta(const Input& input, int itemCount);
UiTabsInput makeUiCycleTabsInput(const Input& input, int itemCount);
int updateUiTabs(
    UiTabsState& state,
    UiContext& ui,
    const UiTabsInput& input,
    int selectedIndex,
    const UiTabItem* items,
    int itemCount,
    const UiRect* rects,
    const UiTabsStyle& style = {});
void drawUiTabs(
    Renderer& renderer,
    const UiTabsState& state,
    int selectedIndex,
    const UiTabItem* items,
    int itemCount,
    const UiRect* rects,
    const UiTabsStyle& style = {});
int updateUiSubTabs(
    UiTabsState& state,
    UiContext& ui,
    const UiTabsInput& input,
    int selectedIndex,
    const UiTabItem* items,
    int itemCount,
    const UiRect* rects,
    const UiSubTabsStyle& style = {});
void drawUiSubTabs(
    Renderer& renderer,
    const UiTabsState& state,
    int selectedIndex,
    const UiTabItem* items,
    int itemCount,
    const UiRect* rects,
    const UiSubTabsStyle& style = {});
int updateUiVerticalTabs(
    UiTabsState& state,
    UiContext& ui,
    const UiTabsInput& input,
    int selectedIndex,
    const UiVerticalTabItem* items,
    int itemCount,
    const UiRect* rects,
    const UiVerticalTabsStyle& style = {});
void drawUiVerticalTabs(
    Renderer& renderer,
    const UiTabsState& state,
    int selectedIndex,
    const UiVerticalTabItem* items,
    int itemCount,
    const UiRect* rects,
    const UiVerticalTabsStyle& style = {});

}
