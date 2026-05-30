#include "engine/InputHelpGlyph.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace majo {

namespace {

enum class GlyphKind {
    Key,
    Mouse,
    PadButton,
    Shoulder,
    Trigger,
    Dpad,
    Stick,
};

enum DirectionBits {
    DirNone = 0,
    DirLeft = 1 << 0,
    DirRight = 1 << 1,
    DirUp = 1 << 2,
    DirDown = 1 << 3,
    DirAll = DirLeft | DirRight | DirUp | DirDown,
};

enum class MousePart {
    Left,
    Right,
    Middle,
};

enum class SemanticGlyph {
    Move,
    NavigateAll,
    NavigateHorizontal,
    NavigateVertical,
    Confirm,
    ConfirmUse,
    AdvanceText,
    Back,
    Use,
    RingAdd,
    RingThrow,
    RingOffset,
    ShortcutCursor,
    RingSwitch,
    Protection,
    GrabPlace,
    ArrangeItems,
    RingRemoveAll,
    ShortcutRow,
    Inventory,
    Pause,
};

struct Glyph {
    GlyphKind kind = GlyphKind::Key;
    std::string label;
    int directions = DirNone;
    MousePart mousePart = MousePart::Left;
    Color accent{154, 190, 255, 255};
};

struct Segment {
    std::string text;
    std::vector<Glyph> glyphs;
    bool newline = false;
};

const Input* currentInput = nullptr;

Color withAlpha(Color color, unsigned char alpha)
{
    color.a = alpha;
    return color;
}

Color scaleAlpha(Color color, float scale)
{
    color.a = static_cast<unsigned char>(std::clamp(std::lround(static_cast<float>(color.a) * scale), 0L, 255L));
    return color;
}

bool isAsciiAlphaNum(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) != 0;
}

bool hasAsciiBoundary(std::string_view text, std::size_t offset, std::size_t length)
{
    const bool beforeOk = offset == 0 || !isAsciiAlphaNum(text[offset - 1]);
    const bool afterOk = offset + length >= text.size() || !isAsciiAlphaNum(text[offset + length]);
    return beforeOk && afterOk;
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

void popUtf8Codepoint(std::string& text)
{
    if (text.empty()) {
        return;
    }
    text.pop_back();
    while (!text.empty() && (static_cast<unsigned char>(text.back()) & 0xc0U) == 0x80U) {
        text.pop_back();
    }
}

bool startsWithAt(std::string_view text, std::size_t offset, std::string_view pattern)
{
    return offset + pattern.size() <= text.size() && text.substr(offset, pattern.size()) == pattern;
}

InputDeviceKind activeDevice(const Input* input)
{
    return input == nullptr ? InputDeviceKind::KeyboardMouse : input->lastActiveDevice();
}

const InputBindingMap& activeBindings(const Input* input)
{
    if (input != nullptr) {
        return input->bindingMap();
    }
    static const InputBindingMap defaults = defaultInputBindings();
    return defaults;
}

std::string keyboardLabel(int scancode)
{
    switch (static_cast<SDL_Scancode>(scancode)) {
    case SDL_SCANCODE_RETURN:
    case SDL_SCANCODE_KP_ENTER:
        return "Enter";
    case SDL_SCANCODE_ESCAPE:
        return "Esc";
    case SDL_SCANCODE_BACKSPACE:
        return "Back";
    case SDL_SCANCODE_DELETE:
        return "Del";
    case SDL_SCANCODE_SPACE:
        return "Space";
    case SDL_SCANCODE_TAB:
        return "Tab";
    case SDL_SCANCODE_LSHIFT:
    case SDL_SCANCODE_RSHIFT:
        return "Shift";
    case SDL_SCANCODE_LCTRL:
    case SDL_SCANCODE_RCTRL:
        return "Ctrl";
    case SDL_SCANCODE_LALT:
    case SDL_SCANCODE_RALT:
        return "Alt";
    default:
        break;
    }

    std::string label = keyboardScancodeName(scancode);
    if (label.size() == 1) {
        label[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(label[0])));
    }
    return label;
}

Glyph keyGlyph(std::string label)
{
    Glyph glyph;
    glyph.kind = GlyphKind::Key;
    glyph.label = std::move(label);
    glyph.accent = {150, 194, 255, 255};
    return glyph;
}

Glyph dpadGlyph(int directions = DirAll)
{
    Glyph glyph;
    glyph.kind = GlyphKind::Dpad;
    glyph.directions = directions;
    glyph.accent = {151, 218, 255, 255};
    return glyph;
}

Glyph stickGlyph(std::string label = "L", int directions = DirAll)
{
    Glyph glyph;
    glyph.kind = GlyphKind::Stick;
    glyph.label = std::move(label);
    glyph.directions = directions;
    glyph.accent = {170, 228, 198, 255};
    return glyph;
}

Glyph mouseGlyph(MousePart part)
{
    Glyph glyph;
    glyph.kind = GlyphKind::Mouse;
    glyph.mousePart = part;
    glyph.accent = {205, 214, 232, 255};
    return glyph;
}

Glyph padButtonGlyph(std::string label, Color accent)
{
    Glyph glyph;
    glyph.kind = GlyphKind::PadButton;
    glyph.label = std::move(label);
    glyph.accent = accent;
    return glyph;
}

Glyph shoulderGlyph(std::string label)
{
    Glyph glyph;
    glyph.kind = GlyphKind::Shoulder;
    glyph.label = std::move(label);
    glyph.accent = {172, 194, 226, 255};
    return glyph;
}

Glyph triggerGlyph(std::string label)
{
    Glyph glyph;
    glyph.kind = GlyphKind::Trigger;
    glyph.label = std::move(label);
    glyph.accent = {184, 206, 238, 255};
    return glyph;
}

std::string glyphKey(const Glyph& glyph)
{
    return std::to_string(static_cast<int>(glyph.kind)) + ":" +
        glyph.label + ":" +
        std::to_string(glyph.directions) + ":" +
        std::to_string(static_cast<int>(glyph.mousePart));
}

void appendGlyph(std::vector<Glyph>& glyphs, Glyph glyph)
{
    for (Glyph& existing : glyphs) {
        if (existing.kind == GlyphKind::Dpad && glyph.kind == GlyphKind::Dpad) {
            existing.directions |= glyph.directions;
            return;
        }
        if (existing.kind == GlyphKind::Stick && glyph.kind == GlyphKind::Stick && existing.label == glyph.label) {
            existing.directions |= glyph.directions;
            return;
        }
        if (glyphKey(existing) == glyphKey(glyph)) {
            return;
        }
    }
    glyphs.push_back(std::move(glyph));
}

std::optional<Glyph> glyphFromBinding(const InputBinding& binding)
{
    switch (binding.device) {
    case InputBindingDevice::Keyboard:
        switch (static_cast<SDL_Scancode>(binding.code)) {
        case SDL_SCANCODE_LEFT: return dpadGlyph(DirLeft);
        case SDL_SCANCODE_RIGHT: return dpadGlyph(DirRight);
        case SDL_SCANCODE_UP: return dpadGlyph(DirUp);
        case SDL_SCANCODE_DOWN: return dpadGlyph(DirDown);
        default: return keyGlyph(keyboardLabel(binding.code));
        }
    case InputBindingDevice::MouseButton:
        if (binding.code == SDL_BUTTON_RIGHT) {
            return mouseGlyph(MousePart::Right);
        }
        if (binding.code == SDL_BUTTON_MIDDLE) {
            return mouseGlyph(MousePart::Middle);
        }
        return mouseGlyph(MousePart::Left);
    case InputBindingDevice::GamepadButton:
        switch (static_cast<SDL_GamepadButton>(binding.code)) {
        case SDL_GAMEPAD_BUTTON_SOUTH: return padButtonGlyph("A", {98, 220, 144, 255});
        case SDL_GAMEPAD_BUTTON_EAST: return padButtonGlyph("B", {244, 116, 116, 255});
        case SDL_GAMEPAD_BUTTON_WEST: return padButtonGlyph("X", {110, 174, 255, 255});
        case SDL_GAMEPAD_BUTTON_NORTH: return padButtonGlyph("Y", {248, 210, 96, 255});
        case SDL_GAMEPAD_BUTTON_BACK: return shoulderGlyph("View");
        case SDL_GAMEPAD_BUTTON_START: return shoulderGlyph("Menu");
        case SDL_GAMEPAD_BUTTON_LEFT_STICK: return stickGlyph("L3", DirAll);
        case SDL_GAMEPAD_BUTTON_RIGHT_STICK: return stickGlyph("R3", DirAll);
        case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return shoulderGlyph("LB");
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return shoulderGlyph("RB");
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return dpadGlyph(DirLeft);
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return dpadGlyph(DirRight);
        case SDL_GAMEPAD_BUTTON_DPAD_UP: return dpadGlyph(DirUp);
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return dpadGlyph(DirDown);
        default: return shoulderGlyph(gamepadButtonName(binding.code));
        }
    case InputBindingDevice::GamepadAxis:
        switch (static_cast<SDL_GamepadAxis>(binding.code)) {
        case SDL_GAMEPAD_AXIS_LEFTX: return stickGlyph("L", binding.direction < 0 ? DirLeft : DirRight);
        case SDL_GAMEPAD_AXIS_LEFTY: return stickGlyph("L", binding.direction < 0 ? DirUp : DirDown);
        case SDL_GAMEPAD_AXIS_RIGHTX: return stickGlyph("R", binding.direction < 0 ? DirLeft : DirRight);
        case SDL_GAMEPAD_AXIS_RIGHTY: return stickGlyph("R", binding.direction < 0 ? DirUp : DirDown);
        case SDL_GAMEPAD_AXIS_LEFT_TRIGGER: return triggerGlyph("LT");
        case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: return triggerGlyph("RT");
        default: return triggerGlyph(gamepadAxisName(binding.code));
        }
    }
    return std::nullopt;
}

bool bindingMatchesDevice(const InputBinding& binding, InputDeviceKind device)
{
    if (device == InputDeviceKind::Gamepad) {
        return binding.device == InputBindingDevice::GamepadButton ||
            binding.device == InputBindingDevice::GamepadAxis;
    }
    return binding.device == InputBindingDevice::Keyboard ||
        binding.device == InputBindingDevice::MouseButton;
}

std::vector<Glyph> glyphsForActions(const Input* input, std::initializer_list<InputAction> actions)
{
    std::vector<Glyph> result;
    const InputBindingMap& bindings = activeBindings(input);
    const InputDeviceKind primary = activeDevice(input);
    const std::array<InputDeviceKind, 2> deviceOrder{
        primary,
        primary == InputDeviceKind::Gamepad ? InputDeviceKind::KeyboardMouse : InputDeviceKind::Gamepad,
    };

    for (InputDeviceKind device : deviceOrder) {
        const std::size_t beforeDevice = result.size();
        for (InputAction action : actions) {
            const std::vector<InputBinding>& actionBindings = bindings[inputActionIndex(action)];
            for (const InputBinding& binding : actionBindings) {
                if (!bindingMatchesDevice(binding, device)) {
                    continue;
                }
                if (std::optional<Glyph> glyph = glyphFromBinding(binding)) {
                    appendGlyph(result, *glyph);
                    break;
                }
            }
        }
        if (result.size() > beforeDevice) {
            break;
        }
    }

    return result;
}

std::vector<Glyph> semanticGlyphs(SemanticGlyph semantic, const Input* input)
{
    const bool gamepad = activeDevice(input) == InputDeviceKind::Gamepad;
    switch (semantic) {
    case SemanticGlyph::Move:
        return {gamepad ? stickGlyph("L", DirAll) : dpadGlyph(DirAll)};
    case SemanticGlyph::NavigateAll:
        return {dpadGlyph(DirAll)};
    case SemanticGlyph::NavigateHorizontal:
        return {dpadGlyph(DirLeft | DirRight)};
    case SemanticGlyph::NavigateVertical:
        return {dpadGlyph(DirUp | DirDown)};
    case SemanticGlyph::Confirm:
        return glyphsForActions(input, {InputAction::Confirm});
    case SemanticGlyph::ConfirmUse:
        return glyphsForActions(input, {InputAction::UseSelectedItem, InputAction::Confirm});
    case SemanticGlyph::AdvanceText:
        return gamepad
            ? glyphsForActions(input, {InputAction::Confirm, InputAction::Cancel})
            : std::vector<Glyph>{keyGlyph("F"), keyGlyph("Enter"), mouseGlyph(MousePart::Left), keyGlyph("Esc")};
    case SemanticGlyph::Back:
        return gamepad
            ? glyphsForActions(input, {InputAction::Cancel, InputAction::Pause})
            : std::vector<Glyph>{keyGlyph("Esc"), mouseGlyph(MousePart::Right)};
    case SemanticGlyph::Use:
        return glyphsForActions(input, {InputAction::UseSelectedItem});
    case SemanticGlyph::RingAdd:
        return glyphsForActions(input, {InputAction::PutSelectedItemOnRing});
    case SemanticGlyph::RingThrow:
        return glyphsForActions(input, {InputAction::ThrowActiveRing});
    case SemanticGlyph::RingOffset:
        return glyphsForActions(input, {InputAction::OffsetRingCenter});
    case SemanticGlyph::ShortcutCursor:
        return glyphsForActions(input, {InputAction::ShortcutCursorLeft, InputAction::ShortcutCursorRight});
    case SemanticGlyph::RingSwitch:
        return glyphsForActions(input, {InputAction::PreviousActiveRing, InputAction::NextActiveRing});
    case SemanticGlyph::Protection:
        return glyphsForActions(input, {InputAction::ToggleProtection});
    case SemanticGlyph::GrabPlace:
        return glyphsForActions(input, {InputAction::GrabOrPlaceItem});
    case SemanticGlyph::ArrangeItems:
        return glyphsForActions(input, {InputAction::ArrangeItems});
    case SemanticGlyph::RingRemoveAll:
        if (gamepad) {
            return glyphsForActions(input, {InputAction::OffsetRingCenter, InputAction::PutSelectedItemOnRing});
        } else {
            std::vector<Glyph> result{keyGlyph("Shift")};
            std::vector<Glyph> removeGlyphs = glyphsForActions(input, {InputAction::PutSelectedItemOnRing});
            result.insert(result.end(), removeGlyphs.begin(), removeGlyphs.end());
            return result;
        }
    case SemanticGlyph::ShortcutRow:
        return glyphsForActions(input, {InputAction::ToggleShortcutRow});
    case SemanticGlyph::Inventory:
        return glyphsForActions(input, {InputAction::OpenInventory});
    case SemanticGlyph::Pause:
        return glyphsForActions(input, {InputAction::Pause});
    }
    return {};
}

bool matchLiteral(std::string_view text, std::size_t offset, std::string_view pattern, std::size_t& outEnd)
{
    if (!startsWithAt(text, offset, pattern)) {
        return false;
    }
    outEnd = offset + pattern.size();
    return true;
}

bool matchBoundaryLiteral(std::string_view text, std::size_t offset, std::string_view pattern, std::size_t& outEnd)
{
    if (!startsWithAt(text, offset, pattern) || !hasAsciiBoundary(text, offset, pattern.size())) {
        return false;
    }
    outEnd = offset + pattern.size();
    return true;
}

bool matchExplicitTag(std::string_view text, std::size_t offset, const Input* input, std::size_t& outEnd, std::vector<Glyph>& outGlyphs)
{
    if (offset >= text.size() || text[offset] != '{') {
        return false;
    }
    const std::size_t close = text.find('}', offset + 1);
    if (close == std::string_view::npos) {
        return false;
    }

    const std::string_view body = text.substr(offset + 1, close - offset - 1);
    if (body == "move") {
        outGlyphs = semanticGlyphs(SemanticGlyph::Move, input);
    } else if (body == "nav") {
        outGlyphs = semanticGlyphs(SemanticGlyph::NavigateAll, input);
    } else if (body.rfind("act:", 0) == 0) {
        if (std::optional<InputAction> action = parseInputAction(body.substr(4))) {
            outGlyphs = glyphsForActions(input, {*action});
        }
    } else if (body.rfind("key:", 0) == 0) {
        outGlyphs = {keyGlyph(std::string(body.substr(4)))};
    } else if (body.rfind("mouse:", 0) == 0) {
        const std::string_view part = body.substr(6);
        outGlyphs = {mouseGlyph(part == "right" ? MousePart::Right : (part == "middle" ? MousePart::Middle : MousePart::Left))};
    } else if (body.rfind("pad:", 0) == 0) {
        const std::string_view button = body.substr(4);
        if (button == "south") outGlyphs = {padButtonGlyph("A", {98, 220, 144, 255})};
        else if (button == "east") outGlyphs = {padButtonGlyph("B", {244, 116, 116, 255})};
        else if (button == "west") outGlyphs = {padButtonGlyph("X", {110, 174, 255, 255})};
        else if (button == "north") outGlyphs = {padButtonGlyph("Y", {248, 210, 96, 255})};
    }

    if (outGlyphs.empty()) {
        return false;
    }
    outEnd = close + 1;
    return true;
}

bool matchPlainGlyph(std::string_view text, std::size_t offset, const Input* input, std::size_t& outEnd, std::vector<Glyph>& outGlyphs)
{
    auto semantic = [&](std::string_view pattern, SemanticGlyph kind) {
        if (matchLiteral(text, offset, pattern, outEnd)) {
            outGlyphs = semanticGlyphs(kind, input);
            return true;
        }
        return false;
    };
    auto boundarySemantic = [&](std::string_view pattern, SemanticGlyph kind) {
        if (matchBoundaryLiteral(text, offset, pattern, outEnd)) {
            outGlyphs = semanticGlyphs(kind, input);
            return true;
        }
        return false;
    };
    auto literalGlyphs = [&](std::string_view pattern, std::vector<Glyph> glyphs) {
        if (matchLiteral(text, offset, pattern, outEnd)) {
            outGlyphs = std::move(glyphs);
            return true;
        }
        return false;
    };
    auto boundaryGlyphs = [&](std::string_view pattern, std::vector<Glyph> glyphs) {
        if (matchBoundaryLiteral(text, offset, pattern, outEnd)) {
            outGlyphs = std::move(glyphs);
            return true;
        }
        return false;
    };

    if (semantic("F/Enter/Click/Esc", SemanticGlyph::AdvanceText)) return true;
    if (semantic("F/Enter/クリック/Esc", SemanticGlyph::AdvanceText)) return true;
    if (semantic("WASD/方向キー", SemanticGlyph::Move)) return true;
    if (semantic("AWSD/方向キー", SemanticGlyph::Move)) return true;
    if (semantic("WASD/矢印", SemanticGlyph::Move)) return true;
    if (semantic("AWSD/矢印", SemanticGlyph::Move)) return true;
    if (semantic("方向キー", SemanticGlyph::NavigateAll)) return true;
    if (semantic("矢印", SemanticGlyph::NavigateAll)) return true;
    if (semantic("↑/↓", SemanticGlyph::NavigateVertical)) return true;
    if (semantic("←/→", SemanticGlyph::NavigateHorizontal)) return true;
    if (semantic("F/Enter", SemanticGlyph::ConfirmUse)) return true;
    if (semantic("Esc/右クリック", SemanticGlyph::Back)) return true;
    if (semantic("右長押し", SemanticGlyph::RingOffset)) return true;
    if (semantic("Q/E", SemanticGlyph::ShortcutCursor)) return true;
    if (semantic("Z/X", SemanticGlyph::RingSwitch)) return true;
    if (semantic("Shift+R", SemanticGlyph::RingRemoveAll)) return true;
    if (literalGlyphs("Backspace/Delete", {keyGlyph("Back"), keyGlyph("Del")})) return true;
    if (literalGlyphs("Shift+1-3", {keyGlyph("Shift"), keyGlyph("1-3")})) return true;
    if (literalGlyphs("Ctrl+S", {keyGlyph("Ctrl"), keyGlyph("S")})) return true;
    if (literalGlyphs("1〜8", {keyGlyph("1-8")})) return true;
    if (literalGlyphs("1-8", {keyGlyph("1-8")})) return true;
    if (literalGlyphs("1〜3", {keyGlyph("1-3")})) return true;
    if (literalGlyphs("1-3", {keyGlyph("1-3")})) return true;
    if (literalGlyphs("+1/-1", {keyGlyph("+1"), keyGlyph("-1")})) return true;
    if (literalGlyphs("+10/-10", {keyGlyph("+10"), keyGlyph("-10")})) return true;
    if (literalGlyphs("左クリック", {mouseGlyph(MousePart::Left)})) return true;
    if (literalGlyphs("右クリック", {mouseGlyph(MousePart::Right)})) return true;
    if (literalGlyphs("Click", {mouseGlyph(MousePart::Left)})) return true;
    if (literalGlyphs("クリック", {mouseGlyph(MousePart::Left)})) return true;
    if (boundarySemantic("Tab", SemanticGlyph::ShortcutRow)) return true;
    if (boundarySemantic("Enter", SemanticGlyph::Confirm)) return true;
    if (boundarySemantic("Esc", SemanticGlyph::Pause)) return true;
    if (boundaryGlyphs("Space", {keyGlyph("Space")})) return true;
    if (boundaryGlyphs("Backspace", {keyGlyph("Back")})) return true;
    if (boundaryGlyphs("Delete", {keyGlyph("Del")})) return true;
    if (boundarySemantic("F", SemanticGlyph::Use)) return true;
    if (boundarySemantic("R", SemanticGlyph::RingAdd)) return true;
    if (boundarySemantic("P", SemanticGlyph::Protection)) return true;
    if (boundarySemantic("G", SemanticGlyph::GrabPlace)) return true;
    if (boundarySemantic("T", SemanticGlyph::ArrangeItems)) return true;
    if (boundarySemantic("C", SemanticGlyph::RingThrow)) return true;
    if (boundarySemantic("I", SemanticGlyph::Inventory)) return true;

    return false;
}

std::vector<Segment> parseSegments(std::string_view text, const Input* input)
{
    std::vector<Segment> segments;
    std::string pendingText;

    const auto flushText = [&]() {
        if (!pendingText.empty()) {
            segments.push_back(Segment{pendingText, {}, false});
            pendingText.clear();
        }
    };

    for (std::size_t offset = 0; offset < text.size();) {
        if (text[offset] == '\n') {
            flushText();
            segments.push_back(Segment{{}, {}, true});
            ++offset;
            continue;
        }

        std::size_t end = offset;
        std::vector<Glyph> glyphs;
        if (matchExplicitTag(text, offset, input, end, glyphs) ||
            matchPlainGlyph(text, offset, input, end, glyphs)) {
            flushText();
            if (!glyphs.empty()) {
                segments.push_back(Segment{{}, std::move(glyphs), false});
            }
            offset = end;
            continue;
        }

        const std::size_t length = utf8CodepointByteLength(text, offset);
        if (length == 0) {
            break;
        }
        pendingText.append(text.substr(offset, length));
        offset += length;
    }

    flushText();
    return segments;
}

int labelScaleForGlyph(const Glyph& glyph, const InputHelpStyle& style)
{
    if (glyph.label.size() <= 2) {
        return std::max(1, style.scale);
    }
    return std::max(1, style.scale - 1);
}

Vec2 glyphSize(Renderer& renderer, const Glyph& glyph, const InputHelpStyle& style)
{
    const float h = std::max(16.0f, style.iconHeight);
    switch (glyph.kind) {
    case GlyphKind::Key: {
        const int labelScale = labelScaleForGlyph(glyph, style);
        const Vec2 labelSize = renderer.measureText(glyph.label, labelScale);
        return {std::max(h + 8.0f, labelSize.x + 16.0f), h};
    }
    case GlyphKind::Mouse:
        return {h * 0.78f, h};
    case GlyphKind::PadButton:
        return {h, h};
    case GlyphKind::Shoulder:
    case GlyphKind::Trigger: {
        const Vec2 labelSize = renderer.measureText(glyph.label, std::max(1, style.scale - 1));
        return {std::max(h * 1.42f, labelSize.x + 16.0f), h};
    }
    case GlyphKind::Dpad:
        return {h * 1.18f, h};
    case GlyphKind::Stick:
        return {h * 1.2f, h};
    }
    return {h, h};
}

float slashWidth(Renderer& renderer, const InputHelpStyle& style)
{
    return renderer.measureText("/", std::max(1, style.scale - 1)).x + 2.0f;
}

Vec2 glyphGroupSize(Renderer& renderer, const std::vector<Glyph>& glyphs, const InputHelpStyle& style)
{
    float width = 0.0f;
    float height = 0.0f;
    for (std::size_t i = 0; i < glyphs.size(); ++i) {
        const Vec2 size = glyphSize(renderer, glyphs[i], style);
        if (i > 0) {
            width += slashWidth(renderer, style);
        }
        width += size.x;
        height = std::max(height, size.y);
    }
    return {width, height};
}

Vec2 measureLine(Renderer& renderer, const std::vector<Segment>& segments, std::size_t first, std::size_t last, const InputHelpStyle& style)
{
    float width = 0.0f;
    float height = renderer.measureText("0", style.scale).y;
    for (std::size_t i = first; i < last; ++i) {
        const Segment& segment = segments[i];
        if (!segment.text.empty()) {
            const Vec2 size = renderer.measureText(segment.text, style.scale);
            width += size.x;
            height = std::max(height, size.y);
        } else if (!segment.glyphs.empty()) {
            const Vec2 size = glyphGroupSize(renderer, segment.glyphs, style);
            width += size.x;
            height = std::max(height, size.y);
        }
    }
    return {width, height};
}

std::vector<std::pair<std::size_t, std::size_t>> lineRanges(const std::vector<Segment>& segments)
{
    std::vector<std::pair<std::size_t, std::size_t>> ranges;
    std::size_t first = 0;
    for (std::size_t i = 0; i < segments.size(); ++i) {
        if (segments[i].newline) {
            ranges.emplace_back(first, i);
            first = i + 1;
        }
    }
    ranges.emplace_back(first, segments.size());
    return ranges;
}

void fillSoftRoundedRect(Renderer& renderer, Vec2 pos, Vec2 size, float radius, Color color)
{
    if (size.x <= 0.0f || size.y <= 0.0f || color.a == 0) {
        return;
    }
    const float r = clamp(radius, 0.0f, std::min(size.x, size.y) * 0.5f);
    if (r <= 0.0f) {
        renderer.fillRect(pos, size, color);
        return;
    }

    renderer.fillRect({pos.x + r, pos.y}, {std::max(0.0f, size.x - r * 2.0f), size.y}, color);
    renderer.fillRect({pos.x, pos.y + r}, {r, std::max(0.0f, size.y - r * 2.0f)}, color);
    renderer.fillRect({pos.x + size.x - r, pos.y + r}, {r, std::max(0.0f, size.y - r * 2.0f)}, color);
    renderer.fillSoftCircle({pos.x + r, pos.y + r}, r, color);
    renderer.fillSoftCircle({pos.x + size.x - r, pos.y + r}, r, color);
    renderer.fillSoftCircle({pos.x + r, pos.y + size.y - r}, r, color);
    renderer.fillSoftCircle({pos.x + size.x - r, pos.y + size.y - r}, r, color);
}

void drawCenteredText(Renderer& renderer, Vec2 pos, Vec2 size, std::string_view label, Color color, int scale)
{
    const Vec2 labelSize = renderer.measureText(label, scale);
    renderer.drawText(
        {pos.x + (size.x - labelSize.x) * 0.5f, pos.y + (size.y - labelSize.y) * 0.5f - 1.0f},
        label,
        color,
        scale);
}

void drawKey(Renderer& renderer, Vec2 pos, Vec2 size, const Glyph& glyph, const InputHelpStyle& style)
{
    const float r = std::min(size.y * 0.33f, 7.0f);
    fillSoftRoundedRect(renderer, pos + Vec2{0.0f, 2.0f}, size, r, {0, 0, 0, 90});
    fillSoftRoundedRect(renderer, pos, size, r, {214, 224, 244, 236});
    fillSoftRoundedRect(renderer, pos + Vec2{2.0f, 2.0f}, size - Vec2{4.0f, 5.0f}, std::max(1.0f, r - 2.0f), {40, 50, 72, 245});
    renderer.drawSoftLine(
        pos + Vec2{r + 2.0f, 3.0f},
        pos + Vec2{size.x - r - 2.0f, 3.0f},
        1.6f,
        {255, 255, 255, 118});
    drawCenteredText(renderer, pos, size - Vec2{0.0f, 1.0f}, glyph.label, {246, 250, 255, 255}, labelScaleForGlyph(glyph, style));
}

void drawPadButton(Renderer& renderer, Vec2 pos, Vec2 size, const Glyph& glyph, const InputHelpStyle& style)
{
    const Vec2 center = pos + size * 0.5f;
    const float radius = std::min(size.x, size.y) * 0.5f;
    renderer.fillSoftCircle(center + Vec2{0.0f, 2.0f}, radius, {0, 0, 0, 86});
    renderer.fillSoftCircle(center, radius, {28, 34, 48, 246});
    renderer.drawSoftRing(center, radius - 1.5f, 3.0f, scaleAlpha(glyph.accent, 0.82f));
    renderer.fillSoftCircle(center + Vec2{-radius * 0.22f, -radius * 0.26f}, radius * 0.22f, {255, 255, 255, 72});
    drawCenteredText(renderer, pos, size, glyph.label, {255, 255, 255, 255}, std::max(1, style.scale));
}

void drawShoulder(Renderer& renderer, Vec2 pos, Vec2 size, const Glyph& glyph, const InputHelpStyle& style)
{
    const float r = size.y * 0.45f;
    fillSoftRoundedRect(renderer, pos + Vec2{0.0f, 2.0f}, size, r, {0, 0, 0, 80});
    fillSoftRoundedRect(renderer, pos, size, r, {198, 210, 232, 228});
    fillSoftRoundedRect(renderer, pos + Vec2{2.0f, 2.0f}, size - Vec2{4.0f, 5.0f}, r - 2.0f, {36, 44, 64, 248});
    renderer.drawSoftLine(pos + Vec2{r * 0.55f, 3.0f}, pos + Vec2{size.x - r * 0.55f, 3.0f}, 1.5f, {255, 255, 255, 105});
    drawCenteredText(renderer, pos, size, glyph.label, {244, 248, 255, 255}, std::max(1, style.scale - 1));
}

void drawMouse(Renderer& renderer, Vec2 pos, Vec2 size, const Glyph& glyph)
{
    const float r = size.x * 0.5f;
    fillSoftRoundedRect(renderer, pos + Vec2{0.0f, 2.0f}, size, r, {0, 0, 0, 76});
    fillSoftRoundedRect(renderer, pos, size, r, {216, 226, 244, 230});
    fillSoftRoundedRect(renderer, pos + Vec2{2.0f, 2.0f}, size - Vec2{4.0f, 4.0f}, std::max(1.0f, r - 2.0f), {42, 50, 66, 248});
    const float splitX = pos.x + size.x * 0.5f;
    const float top = pos.y + 4.0f;
    const float midY = pos.y + size.y * 0.47f;
    renderer.drawSoftLine({splitX, top}, {splitX, midY}, 1.2f, {204, 214, 232, 170});
    renderer.drawSoftLine({pos.x + 5.0f, midY}, {pos.x + size.x - 5.0f, midY}, 1.2f, {204, 214, 232, 116});
    Vec2 activePos{};
    Vec2 activeSize{};
    const Color activeColor{242, 248, 255, 126};
    if (glyph.mousePart == MousePart::Left) {
        activePos = {pos.x + 3.0f, pos.y + 3.0f};
        activeSize = {size.x * 0.5f - 3.5f, size.y * 0.38f};
    } else if (glyph.mousePart == MousePart::Right) {
        activePos = {splitX + 0.5f, pos.y + 3.0f};
        activeSize = {size.x * 0.5f - 3.5f, size.y * 0.38f};
    } else {
        activePos = {splitX - 2.0f, pos.y + 4.0f};
        activeSize = {4.0f, size.y * 0.28f};
    }
    fillSoftRoundedRect(renderer, activePos, activeSize, 2.0f, activeColor);
}

void drawDpadButton(Renderer& renderer, Vec2 pos, Vec2 size, bool active, Color accent)
{
    const float r = std::min(size.x, size.y) * 0.22f;
    fillSoftRoundedRect(renderer, pos + Vec2{0.0f, 1.0f}, size, r, {0, 0, 0, 64});
    fillSoftRoundedRect(renderer, pos, size, r, active ? scaleAlpha(accent, 0.9f) : Color{54, 62, 78, 238});
    fillSoftRoundedRect(renderer, pos + Vec2{1.6f, 1.6f}, size - Vec2{3.2f, 3.8f}, std::max(1.0f, r - 1.6f), active ? Color{45, 72, 96, 245} : Color{28, 34, 48, 245});
}

void drawDpad(Renderer& renderer, Vec2 pos, Vec2 size, const Glyph& glyph)
{
    const float unit = std::min(size.x / 3.0f, size.y / 3.0f);
    const Vec2 cell{unit, unit};
    const Vec2 origin{pos.x + (size.x - unit * 3.0f) * 0.5f, pos.y + (size.y - unit * 3.0f) * 0.5f};
    drawDpadButton(renderer, origin + Vec2{unit, 0.0f}, cell, (glyph.directions & DirUp) != 0, glyph.accent);
    drawDpadButton(renderer, origin + Vec2{0.0f, unit}, cell, (glyph.directions & DirLeft) != 0, glyph.accent);
    drawDpadButton(renderer, origin + Vec2{unit, unit}, cell, false, glyph.accent);
    drawDpadButton(renderer, origin + Vec2{unit * 2.0f, unit}, cell, (glyph.directions & DirRight) != 0, glyph.accent);
    drawDpadButton(renderer, origin + Vec2{unit, unit * 2.0f}, cell, (glyph.directions & DirDown) != 0, glyph.accent);
}

void drawStick(Renderer& renderer, Vec2 pos, Vec2 size, const Glyph& glyph, const InputHelpStyle& style)
{
    const Vec2 center = pos + size * 0.5f;
    const float radius = std::min(size.x, size.y) * 0.42f;
    renderer.fillSoftCircle(center + Vec2{0.0f, 2.0f}, radius + 1.0f, {0, 0, 0, 78});
    renderer.drawSoftRing(center, radius, 4.0f, {190, 204, 226, 220});
    renderer.fillSoftCircle(center, radius * 0.68f, {28, 36, 52, 244});
    const Vec2 knobOffset{
        ((glyph.directions & DirRight) != 0 ? 1.0f : 0.0f) + ((glyph.directions & DirLeft) != 0 ? -1.0f : 0.0f),
        ((glyph.directions & DirDown) != 0 ? 1.0f : 0.0f) + ((glyph.directions & DirUp) != 0 ? -1.0f : 0.0f),
    };
    Vec2 knob = center;
    if (lengthSquared(knobOffset) > 0.0f && glyph.directions != DirAll) {
        knob += normalize(knobOffset) * (radius * 0.26f);
    }
    renderer.fillSoftCircle(knob, radius * 0.34f, scaleAlpha(glyph.accent, 0.95f));
    if (!glyph.label.empty()) {
        drawCenteredText(renderer, {pos.x, pos.y + size.y * 0.47f}, {size.x, size.y * 0.48f}, glyph.label, {240, 248, 255, 238}, std::max(1, style.scale - 1));
    }
}

void drawGlyph(Renderer& renderer, Vec2 pos, const Glyph& glyph, const InputHelpStyle& style)
{
    const Vec2 size = glyphSize(renderer, glyph, style);
    switch (glyph.kind) {
    case GlyphKind::Key:
        drawKey(renderer, pos, size, glyph, style);
        break;
    case GlyphKind::Mouse:
        drawMouse(renderer, pos, size, glyph);
        break;
    case GlyphKind::PadButton:
        drawPadButton(renderer, pos, size, glyph, style);
        break;
    case GlyphKind::Shoulder:
    case GlyphKind::Trigger:
        drawShoulder(renderer, pos, size, glyph, style);
        break;
    case GlyphKind::Dpad:
        drawDpad(renderer, pos, size, glyph);
        break;
    case GlyphKind::Stick:
        drawStick(renderer, pos, size, glyph, style);
        break;
    }
}

void drawTextRun(Renderer& renderer, Vec2 pos, std::string_view text, const InputHelpStyle& style)
{
    if (style.outlineEnabled) {
        renderer.drawOutlinedText(pos, text, style.text, style.outline, style.outlinePx, style.scale);
    } else {
        renderer.drawText(pos, text, style.text, style.scale);
    }
}

void drawGlyphGroup(Renderer& renderer, Vec2 pos, const std::vector<Glyph>& glyphs, const InputHelpStyle& style)
{
    Vec2 cursor = pos;
    const int slashScale = std::max(1, style.scale - 1);
    for (std::size_t i = 0; i < glyphs.size(); ++i) {
        if (i > 0) {
            const Vec2 slashSize = renderer.measureText("/", slashScale);
            const float slashY = pos.y + std::max(0.0f, (style.iconHeight - slashSize.y) * 0.5f);
            renderer.drawText({cursor.x + 1.0f, slashY}, "/", style.text, slashScale);
            cursor.x += slashWidth(renderer, style);
        }
        drawGlyph(renderer, cursor, glyphs[i], style);
        cursor.x += glyphSize(renderer, glyphs[i], style).x;
    }
}

} // namespace

void setInputHelpContext(const Input* input)
{
    currentInput = input;
}

const Input* inputHelpContext()
{
    return currentInput;
}

Vec2 measureInputHelpText(Renderer& renderer, std::string_view text, const InputHelpStyle& style, const Input* input)
{
    const Input* resolvedInput = input != nullptr ? input : currentInput;
    const std::vector<Segment> segments = parseSegments(text, resolvedInput);
    const auto ranges = lineRanges(segments);
    Vec2 result{};
    for (const auto& range : ranges) {
        const Vec2 line = measureLine(renderer, segments, range.first, range.second, style);
        result.x = std::max(result.x, line.x);
        result.y += line.y;
    }
    if (ranges.size() > 1) {
        result.y += std::max(0.0f, static_cast<float>(ranges.size() - 1) * 2.0f);
    }
    return result;
}

std::string fittedInputHelpText(Renderer& renderer, std::string text, float maxWidth, const InputHelpStyle& style, const Input* input)
{
    if (maxWidth <= 0.0f) {
        return "";
    }
    if (measureInputHelpText(renderer, text, style, input).x <= maxWidth) {
        return text;
    }

    constexpr std::string_view Ellipsis = "...";
    while (!text.empty()) {
        if (text.back() == '}') {
            const std::size_t open = text.rfind('{');
            if (open != std::string::npos) {
                text.erase(open);
            } else {
                popUtf8Codepoint(text);
            }
        } else {
            popUtf8Codepoint(text);
        }
        std::string candidate = text + std::string(Ellipsis);
        if (measureInputHelpText(renderer, candidate, style, input).x <= maxWidth) {
            return candidate;
        }
    }
    return measureInputHelpText(renderer, Ellipsis, style, input).x <= maxWidth ? std::string(Ellipsis) : "";
}

void drawInputHelpText(Renderer& renderer, Vec2 pos, std::string_view text, const InputHelpStyle& style, const Input* input)
{
    const Input* resolvedInput = input != nullptr ? input : currentInput;
    const std::vector<Segment> segments = parseSegments(text, resolvedInput);
    const auto ranges = lineRanges(segments);
    float y = pos.y;
    for (const auto& range : ranges) {
        const Vec2 lineSize = measureLine(renderer, segments, range.first, range.second, style);
        Vec2 cursor{pos.x, y};
        for (std::size_t i = range.first; i < range.second; ++i) {
            const Segment& segment = segments[i];
            if (!segment.text.empty()) {
                const Vec2 size = renderer.measureText(segment.text, style.scale);
                drawTextRun(renderer, {cursor.x, y + (lineSize.y - size.y) * 0.5f}, segment.text, style);
                cursor.x += size.x;
            } else if (!segment.glyphs.empty()) {
                const Vec2 size = glyphGroupSize(renderer, segment.glyphs, style);
                drawGlyphGroup(renderer, {cursor.x, y + (lineSize.y - size.y) * 0.5f}, segment.glyphs, style);
                cursor.x += size.x;
            }
        }
        y += lineSize.y + 2.0f;
    }
}

} // namespace majo
