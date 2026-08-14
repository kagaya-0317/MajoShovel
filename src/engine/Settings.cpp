#include "engine/Settings.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace majo {

namespace {

constexpr int CurrentSettingsVersion = 9;
constexpr int MinWindowWidth = 640;
constexpr int MinWindowHeight = 360;
constexpr int MaxWindowWidth = 7680;
constexpr int MaxWindowHeight = 4320;

std::string lowerAscii(std::string_view text)
{
    std::string result(text);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

float clampVolume(float value)
{
    if (!(value >= 0.0f && value <= 1.0f)) {
        return value < 0.0f ? 0.0f : 1.0f;
    }
    return value;
}

void setError(std::string* outError, std::string message)
{
    if (outError != nullptr) {
        *outError = std::move(message);
    }
}

std::filesystem::path settingsRootPath()
{
#if defined(_WIN32)
    char* localAppData = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&localAppData, &length, "LOCALAPPDATA") == 0) {
        if (localAppData != nullptr && localAppData[0] != '\0') {
            const std::filesystem::path path = std::filesystem::path(localAppData) / "MajoShovel";
            std::free(localAppData);
            return path;
        }
        if (localAppData != nullptr) {
            std::free(localAppData);
        }
    }
#else
    const char* localAppData = std::getenv("LOCALAPPDATA");
    if (localAppData != nullptr && localAppData[0] != '\0') {
        return std::filesystem::path(localAppData) / "MajoShovel";
    }
#endif
    return std::filesystem::path(".local") / "MajoShovel";
}

struct JsonValue {
    enum class Type {
        Null,
        Bool,
        Number,
        String,
        Object,
        Array,
    };

    Type type = Type::Null;
    bool boolValue = false;
    double numberValue = 0.0;
    std::string stringValue;
    std::unordered_map<std::string, JsonValue> objectValue;
    std::vector<JsonValue> arrayValue;

    const JsonValue* member(std::string_view name) const
    {
        if (type != Type::Object) {
            return nullptr;
        }
        const auto it = objectValue.find(std::string(name));
        return it != objectValue.end() ? &it->second : nullptr;
    }
};

class JsonParser {
public:
    explicit JsonParser(std::string_view text) : text_(text) {}

    std::optional<JsonValue> parse(std::string& outError)
    {
        skipWhitespace();
        std::optional<JsonValue> value = parseValue();
        if (!value) {
            outError = error_;
            return std::nullopt;
        }
        skipWhitespace();
        if (pos_ != text_.size()) {
            outError = "Unexpected trailing characters in settings JSON";
            return std::nullopt;
        }
        return value;
    }

private:
    std::optional<JsonValue> parseValue()
    {
        skipWhitespace();
        if (pos_ >= text_.size()) {
            fail("Unexpected end of settings JSON");
            return std::nullopt;
        }

        const char c = text_[pos_];
        if (c == '{') {
            return parseObject();
        }
        if (c == '[') {
            return parseArray();
        }
        if (c == '"') {
            std::optional<std::string> value = parseString();
            if (!value) {
                return std::nullopt;
            }
            JsonValue result;
            result.type = JsonValue::Type::String;
            result.stringValue = std::move(*value);
            return result;
        }
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c)) != 0) {
            return parseNumber();
        }
        if (matchLiteral("true")) {
            JsonValue result;
            result.type = JsonValue::Type::Bool;
            result.boolValue = true;
            return result;
        }
        if (matchLiteral("false")) {
            JsonValue result;
            result.type = JsonValue::Type::Bool;
            result.boolValue = false;
            return result;
        }
        if (matchLiteral("null")) {
            JsonValue result;
            result.type = JsonValue::Type::Null;
            return result;
        }

        fail("Unexpected token in settings JSON");
        return std::nullopt;
    }

    std::optional<JsonValue> parseObject()
    {
        JsonValue result;
        result.type = JsonValue::Type::Object;
        ++pos_;
        skipWhitespace();
        if (consume('}')) {
            return result;
        }

        while (pos_ < text_.size()) {
            skipWhitespace();
            std::optional<std::string> key = parseString();
            if (!key) {
                return std::nullopt;
            }
            skipWhitespace();
            if (!consume(':')) {
                fail("Expected ':' in settings JSON object");
                return std::nullopt;
            }
            std::optional<JsonValue> value = parseValue();
            if (!value) {
                return std::nullopt;
            }
            result.objectValue[*key] = std::move(*value);
            skipWhitespace();
            if (consume('}')) {
                return result;
            }
            if (!consume(',')) {
                fail("Expected ',' or '}' in settings JSON object");
                return std::nullopt;
            }
        }

        fail("Unterminated settings JSON object");
        return std::nullopt;
    }

    std::optional<JsonValue> parseArray()
    {
        JsonValue result;
        result.type = JsonValue::Type::Array;
        ++pos_;
        skipWhitespace();
        if (consume(']')) {
            return result;
        }

        while (pos_ < text_.size()) {
            std::optional<JsonValue> value = parseValue();
            if (!value) {
                return std::nullopt;
            }
            result.arrayValue.push_back(std::move(*value));
            skipWhitespace();
            if (consume(']')) {
                return result;
            }
            if (!consume(',')) {
                fail("Expected ',' or ']' in settings JSON array");
                return std::nullopt;
            }
        }

        fail("Unterminated settings JSON array");
        return std::nullopt;
    }

    std::optional<std::string> parseString()
    {
        if (!consume('"')) {
            fail("Expected string in settings JSON");
            return std::nullopt;
        }

        std::string result;
        while (pos_ < text_.size()) {
            const char c = text_[pos_++];
            if (c == '"') {
                return result;
            }
            if (c != '\\') {
                result.push_back(c);
                continue;
            }
            if (pos_ >= text_.size()) {
                fail("Unterminated escape in settings JSON string");
                return std::nullopt;
            }
            const char escaped = text_[pos_++];
            switch (escaped) {
            case '"': result.push_back('"'); break;
            case '\\': result.push_back('\\'); break;
            case '/': result.push_back('/'); break;
            case 'b': result.push_back('\b'); break;
            case 'f': result.push_back('\f'); break;
            case 'n': result.push_back('\n'); break;
            case 'r': result.push_back('\r'); break;
            case 't': result.push_back('\t'); break;
            case 'u':
                if (pos_ + 4 > text_.size()) {
                    fail("Invalid unicode escape in settings JSON string");
                    return std::nullopt;
                }
                pos_ += 4;
                result.push_back('?');
                break;
            default:
                fail("Invalid escape in settings JSON string");
                return std::nullopt;
            }
        }

        fail("Unterminated settings JSON string");
        return std::nullopt;
    }

    std::optional<JsonValue> parseNumber()
    {
        const std::size_t start = pos_;
        if (text_[pos_] == '-') {
            ++pos_;
        }
        while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_])) != 0) {
            ++pos_;
        }
        if (pos_ < text_.size() && text_[pos_] == '.') {
            ++pos_;
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_])) != 0) {
                ++pos_;
            }
        }
        if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) {
                ++pos_;
            }
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_])) != 0) {
                ++pos_;
            }
        }

        try {
            JsonValue result;
            result.type = JsonValue::Type::Number;
            result.numberValue = std::stod(std::string(text_.substr(start, pos_ - start)));
            return result;
        } catch (const std::exception&) {
            fail("Invalid number in settings JSON");
            return std::nullopt;
        }
    }

    void skipWhitespace()
    {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_])) != 0) {
            ++pos_;
        }
    }

    bool consume(char expected)
    {
        if (pos_ < text_.size() && text_[pos_] == expected) {
            ++pos_;
            return true;
        }
        return false;
    }

    bool matchLiteral(std::string_view literal)
    {
        if (text_.substr(pos_, literal.size()) == literal) {
            pos_ += literal.size();
            return true;
        }
        return false;
    }

    void fail(std::string message)
    {
        if (error_.empty()) {
            error_ = std::move(message);
        }
    }

    std::string_view text_;
    std::size_t pos_ = 0;
    std::string error_;
};

const JsonValue* objectMember(const JsonValue& value, std::string_view name)
{
    return value.member(name);
}

std::optional<int> intMember(const JsonValue& value, std::string_view name)
{
    const JsonValue* member = objectMember(value, name);
    if (member == nullptr || member->type != JsonValue::Type::Number) {
        return std::nullopt;
    }
    return static_cast<int>(member->numberValue);
}

std::optional<float> floatMember(const JsonValue& value, std::string_view name)
{
    const JsonValue* member = objectMember(value, name);
    if (member == nullptr || member->type != JsonValue::Type::Number) {
        return std::nullopt;
    }
    return static_cast<float>(member->numberValue);
}

std::optional<bool> boolMember(const JsonValue& value, std::string_view name)
{
    const JsonValue* member = objectMember(value, name);
    if (member == nullptr || member->type != JsonValue::Type::Bool) {
        return std::nullopt;
    }
    return member->boolValue;
}

std::optional<std::string> stringMember(const JsonValue& value, std::string_view name)
{
    const JsonValue* member = objectMember(value, name);
    if (member == nullptr || member->type != JsonValue::Type::String) {
        return std::nullopt;
    }
    return member->stringValue;
}

std::string jsonEscape(std::string_view text)
{
    std::string result;
    result.reserve(text.size());
    for (char c : text) {
        switch (c) {
        case '"': result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '\b': result += "\\b"; break;
        case '\f': result += "\\f"; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            result.push_back(c);
            break;
        }
    }
    return result;
}

std::optional<InputBinding> parseInputBindingValue(const JsonValue& value)
{
    if (value.type != JsonValue::Type::Object) {
        return std::nullopt;
    }

    const std::optional<std::string> deviceText = stringMember(value, "device");
    if (!deviceText) {
        return std::nullopt;
    }
    const std::optional<InputBindingDevice> device = parseInputBindingDevice(*deviceText);
    if (!device) {
        return std::nullopt;
    }

    InputBinding binding;
    binding.device = *device;
    if (binding.device == InputBindingDevice::Keyboard) {
        const std::optional<std::string> key = stringMember(value, "key");
        if (!key) {
            return std::nullopt;
        }
        const std::optional<int> scancode = parseKeyboardScancode(*key);
        if (!scancode) {
            return std::nullopt;
        }
        binding.code = *scancode;
        if (const JsonValue* modifiers = objectMember(value, "modifiers")) {
            const auto appendModifier = [&](std::string_view name) {
                if (const std::optional<InputModifiers> modifier = parseInputModifier(name)) {
                    binding.modifiers |= *modifier;
                }
            };
            if (modifiers->type == JsonValue::Type::String) {
                appendModifier(modifiers->stringValue);
            } else if (modifiers->type == JsonValue::Type::Array) {
                for (const JsonValue& modifier : modifiers->arrayValue) {
                    if (modifier.type == JsonValue::Type::String) {
                        appendModifier(modifier.stringValue);
                    }
                }
            }
        }
    } else if (binding.device == InputBindingDevice::MouseButton) {
        const std::optional<std::string> button = stringMember(value, "button");
        if (!button) {
            return std::nullopt;
        }
        const std::optional<int> mouseButton = parseMouseButton(*button);
        if (!mouseButton) {
            return std::nullopt;
        }
        binding.code = *mouseButton;
    } else if (binding.device == InputBindingDevice::GamepadButton) {
        const std::optional<std::string> button = stringMember(value, "button");
        if (!button) {
            return std::nullopt;
        }
        const std::optional<int> gamepadButton = parseGamepadButton(*button);
        if (!gamepadButton) {
            return std::nullopt;
        }
        binding.code = *gamepadButton;
    } else if (binding.device == InputBindingDevice::GamepadAxis) {
        const std::optional<std::string> axis = stringMember(value, "axis");
        if (!axis) {
            return std::nullopt;
        }
        const std::optional<int> gamepadAxis = parseGamepadAxis(*axis);
        if (!gamepadAxis) {
            return std::nullopt;
        }
        binding.code = *gamepadAxis;
        binding.direction = intMember(value, "direction").value_or(1);
        binding.threshold = floatMember(value, "threshold").value_or(0.5f);
    }
    return binding;
}

void loadInputBindings(const JsonValue& inputValue, InputBindingMap& bindings)
{
    const JsonValue* bindingsValue = objectMember(inputValue, "bindings");
    if (bindingsValue == nullptr || bindingsValue->type != JsonValue::Type::Object) {
        return;
    }

    for (const auto& [actionName, actionValue] : bindingsValue->objectValue) {
        const std::optional<InputAction> action = parseInputAction(actionName);
        if (!action || actionValue.type != JsonValue::Type::Array) {
            continue;
        }

        std::vector<InputBinding> parsedBindings;
        for (const JsonValue& bindingValue : actionValue.arrayValue) {
            if (std::optional<InputBinding> binding = parseInputBindingValue(bindingValue)) {
                parsedBindings.push_back(*binding);
            }
        }
        bindings[inputActionIndex(*action)] = std::move(parsedBindings);
    }
}

InputBinding migrationKeyboardBinding(
    std::string_view key,
    InputModifiers modifiers = InputModifiers::None)
{
    return InputBinding{
        .device = InputBindingDevice::Keyboard,
        .code = parseKeyboardScancode(key).value_or(0),
        .modifiers = modifiers,
    };
}

InputBinding migrationMouseBinding(std::string_view button)
{
    return InputBinding{
        .device = InputBindingDevice::MouseButton,
        .code = parseMouseButton(button).value_or(0),
    };
}

InputBinding migrationGamepadButtonBinding(std::string_view button)
{
    return InputBinding{
        .device = InputBindingDevice::GamepadButton,
        .code = parseGamepadButton(button).value_or(-1),
    };
}

InputBinding migrationGamepadAxisBinding(std::string_view axis, int direction, float threshold)
{
    return InputBinding{
        .device = InputBindingDevice::GamepadAxis,
        .code = parseGamepadAxis(axis).value_or(-1),
        .direction = direction,
        .threshold = threshold,
    };
}

bool migrationBindingSetsEqual(
    const std::vector<InputBinding>& lhs,
    const std::vector<InputBinding>& rhs)
{
    if (lhs.size() != rhs.size()) {
        return false;
    }
    std::vector<bool> matched(rhs.size(), false);
    for (const InputBinding& left : lhs) {
        bool found = false;
        for (std::size_t i = 0; i < rhs.size(); ++i) {
            if (!matched[i] && inputBindingEquals(left, rhs[i])) {
                matched[i] = true;
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

void migrateLoadedSettings(GameSettings& settings)
{
    if (settings.version < 2) {
        const std::optional<int> leftTriggerAxis = parseGamepadAxis("left_trigger");
        if (leftTriggerAxis) {
            const InputBinding legacyRingOffsetBinding{
                .device = InputBindingDevice::GamepadAxis,
                .code = *leftTriggerAxis,
                .direction = 1,
                .threshold = 0.45f,
            };
            auto& ringOffsetBindings =
                settings.input.bindings[inputActionIndex(InputAction::OffsetRingCenter)];
            ringOffsetBindings.erase(
                std::remove_if(
                    ringOffsetBindings.begin(),
                    ringOffsetBindings.end(),
                    [&](const InputBinding& binding) {
                        return inputBindingEquals(binding, legacyRingOffsetBinding);
                    }),
                ringOffsetBindings.end());
        }
    }

    if (settings.version < 3) {
        const InputBindingMap currentDefaults = defaultInputBindings();
        const std::vector<InputBinding> legacyShortcutLeft{
            migrationKeyboardBinding("Q"),
            migrationGamepadButtonBinding("dpad_left"),
        };
        const std::vector<InputBinding> legacyShortcutRight{
            migrationKeyboardBinding("E"),
            migrationGamepadButtonBinding("dpad_right"),
        };
        auto& shortcutLeft =
            settings.input.bindings[inputActionIndex(InputAction::ShortcutCursorLeft)];
        if (migrationBindingSetsEqual(shortcutLeft, legacyShortcutLeft)) {
            shortcutLeft = currentDefaults[inputActionIndex(InputAction::ShortcutCursorLeft)];
        }
        auto& shortcutRight =
            settings.input.bindings[inputActionIndex(InputAction::ShortcutCursorRight)];
        if (migrationBindingSetsEqual(shortcutRight, legacyShortcutRight)) {
            shortcutRight = currentDefaults[inputActionIndex(InputAction::ShortcutCursorRight)];
        }

    }

    if (settings.version < 4) {
        const InputBindingMap currentDefaults = defaultInputBindings();
        const std::vector<InputBinding> legacyUse{
            migrationKeyboardBinding("F"),
            migrationGamepadButtonBinding("south"),
        };
        const std::vector<InputBinding> legacyArrange{
            migrationKeyboardBinding("T"),
            migrationGamepadButtonBinding("west"),
        };
        const std::vector<InputBinding> legacyGrab{
            migrationKeyboardBinding("G"),
            migrationGamepadButtonBinding("left_stick"),
            migrationGamepadButtonBinding("right_stick"),
        };

        auto migrateDefault = [&](InputAction action, const std::vector<InputBinding>& legacy) {
            auto& bindings = settings.input.bindings[inputActionIndex(action)];
            if (migrationBindingSetsEqual(bindings, legacy)) {
                bindings = currentDefaults[inputActionIndex(action)];
            }
        };
        migrateDefault(InputAction::UseSelectedItem, legacyUse);
        migrateDefault(InputAction::ArrangeItems, legacyArrange);
        migrateDefault(InputAction::GrabOrPlaceItem, legacyGrab);
    }

    if (settings.version < 5) {
        auto& modifierBindings =
            settings.input.bindings[inputActionIndex(InputAction::SecondaryActionModifier)];
        const bool hasKeyboardBinding = std::any_of(
            modifierBindings.begin(),
            modifierBindings.end(),
            [](const InputBinding& binding) {
                return binding.device == InputBindingDevice::Keyboard;
            });
        if (!hasKeyboardBinding) {
            const auto& defaults =
                defaultInputBindings()[inputActionIndex(InputAction::SecondaryActionModifier)];
            for (const InputBinding& binding : defaults) {
                if (binding.device == InputBindingDevice::Keyboard) {
                    modifierBindings.push_back(binding);
                }
            }
        }
    }

    if (settings.version < 6) {
        const InputBindingMap currentDefaults = defaultInputBindings();
        const auto migrateDefault = [&](InputAction action, const std::vector<InputBinding>& legacy) {
            auto& bindings = settings.input.bindings[inputActionIndex(action)];
            if (migrationBindingSetsEqual(bindings, legacy)) {
                bindings = currentDefaults[inputActionIndex(action)];
            }
        };

        migrateDefault(InputAction::Confirm, {
            migrationKeyboardBinding("Return"),
            migrationKeyboardBinding("Keypad Enter"),
            migrationGamepadButtonBinding("south"),
        });
        migrateDefault(InputAction::Cancel, {
            migrationKeyboardBinding("Backspace"),
            migrationMouseBinding("right"),
            migrationGamepadButtonBinding("east"),
        });
        migrateDefault(InputAction::Pause, {
            migrationKeyboardBinding("Escape"),
            migrationGamepadButtonBinding("start"),
        });

        const std::array<std::pair<InputAction, InputBinding>, 4> legacyRingShiftBindings{{
            {InputAction::ShiftRingLeft, migrationGamepadAxisBinding("rightx", -1, 0.55f)},
            {InputAction::ShiftRingRight, migrationGamepadAxisBinding("rightx", 1, 0.55f)},
            {InputAction::ShiftRingUp, migrationGamepadAxisBinding("righty", -1, 0.55f)},
            {InputAction::ShiftRingDown, migrationGamepadAxisBinding("righty", 1, 0.55f)},
        }};
        for (const auto& [action, legacyBinding] : legacyRingShiftBindings) {
            auto& bindings = settings.input.bindings[inputActionIndex(action)];
            bindings.erase(
                std::remove_if(bindings.begin(), bindings.end(), [&](const InputBinding& binding) {
                    return inputBindingEquals(binding, legacyBinding);
                }),
                bindings.end());
        }
    }

    if (settings.version < 7) {
        const InputBindingMap currentDefaults = defaultInputBindings();
        const auto migrateDefault = [&](InputAction action, const std::vector<InputBinding>& legacy) {
            auto& bindings = settings.input.bindings[inputActionIndex(action)];
            if (migrationBindingSetsEqual(bindings, legacy)) {
                bindings = currentDefaults[inputActionIndex(action)];
            }
        };

        migrateDefault(InputAction::OpenOptions, {
            migrationKeyboardBinding("F9"),
            migrationGamepadButtonBinding("left_shoulder"),
        });
        migrateDefault(InputAction::OpenCredits, {
            migrationKeyboardBinding("F10"),
            migrationGamepadButtonBinding("right_shoulder"),
        });
        migrateDefault(InputAction::UseSelectedItem, {
            migrationKeyboardBinding("F"),
            migrationGamepadButtonBinding("west"),
        });
        migrateDefault(InputAction::PutSelectedItemOnRing, {
            migrationKeyboardBinding("R"),
            migrationGamepadButtonBinding("dpad_down"),
        });
        migrateDefault(InputAction::PreviousShortcutRow, {
            migrationKeyboardBinding("Up", InputModifiers::Shift),
        });
        migrateDefault(InputAction::OpenInventory, {
            migrationKeyboardBinding("I"),
            migrationGamepadButtonBinding("north"),
        });
    }

    if (settings.version < 8) {
        const InputBindingMap currentDefaults = defaultInputBindings();
        const auto migrateDefault = [&](InputAction action, const std::vector<InputBinding>& legacy) {
            auto& bindings = settings.input.bindings[inputActionIndex(action)];
            if (migrationBindingSetsEqual(bindings, legacy)) {
                bindings = currentDefaults[inputActionIndex(action)];
            }
        };

        migrateDefault(InputAction::PreviousShortcutRow, {
            migrationKeyboardBinding("Up", InputModifiers::Shift),
            migrationGamepadButtonBinding("dpad_down"),
        });
        migrateDefault(InputAction::NextShortcutRow, {
            migrationKeyboardBinding("Down", InputModifiers::Shift),
            migrationGamepadButtonBinding("dpad_up"),
        });
    }

    if (settings.version < 9) {
        for (int action = 0; action < InputActionCount; ++action) {
            const InputAction inputAction = static_cast<InputAction>(action);
            if (!inputActionHasPersistentBindings(inputAction)) {
                settings.input.bindings[action].clear();
            }
        }
    }
}

void writeInputBindingJson(std::ostream& out, const InputBinding& binding, std::string_view indent)
{
    out << indent << "{ \"device\": \"" << inputBindingDeviceName(binding.device) << "\"";
    switch (binding.device) {
    case InputBindingDevice::Keyboard:
        out << ", \"key\": \"" << jsonEscape(keyboardScancodeName(binding.code)) << "\"";
        if (binding.modifiers != InputModifiers::None) {
            out << ", \"modifiers\": [";
            bool first = true;
            for (const InputModifiers modifier : {
                    InputModifiers::Ctrl,
                    InputModifiers::Alt,
                    InputModifiers::Shift,
                    InputModifiers::Gui,
                }) {
                if (!inputModifiersContain(binding.modifiers, modifier)) {
                    continue;
                }
                if (!first) {
                    out << ", ";
                }
                first = false;
                out << "\"" << inputModifierName(modifier) << "\"";
            }
            out << "]";
        }
        break;
    case InputBindingDevice::MouseButton:
        out << ", \"button\": \"" << mouseButtonName(binding.code) << "\"";
        break;
    case InputBindingDevice::GamepadButton:
        out << ", \"button\": \"" << gamepadButtonName(binding.code) << "\"";
        break;
    case InputBindingDevice::GamepadAxis:
        out << ", \"axis\": \"" << gamepadAxisName(binding.code) << "\"";
        out << ", \"direction\": " << (binding.direction < 0 ? -1 : 1);
        out << ", \"threshold\": " << binding.threshold;
        break;
    }
    out << " }";
}

std::optional<float> parseScreenBrightnessValue(std::string_view text)
{
    const std::string normalized = lowerAscii(text);
    if (normalized == "dark" || normalized == "darker") {
        return 0.85f;
    }
    if (normalized == "standard" || normalized == "normal" || normalized == "default") {
        return DefaultScreenBrightness;
    }
    if (normalized == "bright" || normalized == "brighter") {
        return 1.15f;
    }

    try {
        std::size_t consumed = 0;
        float value = std::stof(std::string(text), &consumed);
        if (consumed == text.size()) {
            if (value > 2.0f) {
                value *= 0.01f;
            }
            return value;
        }
    } catch (const std::exception&) {
    }
    return std::nullopt;
}

} // namespace

const char* windowModeName(WindowMode mode)
{
    switch (mode) {
    case WindowMode::Windowed: return "windowed";
    case WindowMode::BorderlessFullscreen: return "borderless";
    }
    return "windowed";
}

bool parseWindowMode(std::string_view text, WindowMode& outMode)
{
    const std::string normalized = lowerAscii(text);
    if (normalized == "windowed" || normalized == "window") {
        outMode = WindowMode::Windowed;
        return true;
    }
    if (normalized == "borderless" ||
        normalized == "borderless_fullscreen" ||
        normalized == "borderless-fullscreen" ||
        normalized == "fullscreen") {
        outMode = WindowMode::BorderlessFullscreen;
        return true;
    }
    return false;
}

const char* screenShakeSettingName(ScreenShakeSetting setting)
{
    switch (setting) {
    case ScreenShakeSetting::Off: return "off";
    case ScreenShakeSetting::Low: return "low";
    case ScreenShakeSetting::Standard: return "standard";
    }
    return "standard";
}

bool parseScreenShakeSetting(std::string_view text, ScreenShakeSetting& outSetting)
{
    const std::string normalized = lowerAscii(text);
    if (normalized == "off" || normalized == "none") {
        outSetting = ScreenShakeSetting::Off;
        return true;
    }
    if (normalized == "low" || normalized == "weak") {
        outSetting = ScreenShakeSetting::Low;
        return true;
    }
    if (normalized == "standard" || normalized == "normal" || normalized == "default") {
        outSetting = ScreenShakeSetting::Standard;
        return true;
    }
    return false;
}

const char* inputIconSettingName(InputIconSetting setting)
{
    switch (setting) {
    case InputIconSetting::Auto: return "auto";
    case InputIconSetting::KeyboardMouse: return "keyboard_mouse";
    case InputIconSetting::Gamepad: return "gamepad";
    }
    return "auto";
}

bool parseInputIconSetting(std::string_view text, InputIconSetting& outSetting)
{
    const std::string normalized = lowerAscii(text);
    if (normalized == "auto") {
        outSetting = InputIconSetting::Auto;
        return true;
    }
    if (normalized == "keyboard" || normalized == "keyboard_mouse" || normalized == "keyboard-mouse" || normalized == "mouse") {
        outSetting = InputIconSetting::KeyboardMouse;
        return true;
    }
    if (normalized == "gamepad" || normalized == "pad" || normalized == "controller") {
        outSetting = InputIconSetting::Gamepad;
        return true;
    }
    return false;
}

GameSettings sanitizeSettings(GameSettings settings)
{
    settings.version = CurrentSettingsVersion;
    settings.audio.masterVolume = clampVolume(settings.audio.masterVolume);
    settings.audio.bgmVolume = clampVolume(settings.audio.bgmVolume);
    settings.audio.seVolume = clampVolume(settings.audio.seVolume);
    settings.video.windowWidth = std::clamp(settings.video.windowWidth, MinWindowWidth, MaxWindowWidth);
    settings.video.windowHeight = std::clamp(settings.video.windowHeight, MinWindowHeight, MaxWindowHeight);
    settings.presentation.brightness = std::clamp(settings.presentation.brightness, MinScreenBrightness, MaxScreenBrightness);
    settings.input.bindings = sanitizeInputBindings(settings.input.bindings);
    return settings;
}

SettingsStore::SettingsStore() : path_(defaultPath())
{
}

SettingsStore::SettingsStore(std::filesystem::path path) : path_(std::move(path))
{
}

std::filesystem::path SettingsStore::defaultPath()
{
    return settingsRootPath() / "settings.json";
}

bool SettingsStore::exists() const
{
    std::error_code ec;
    return std::filesystem::exists(path_, ec);
}

bool SettingsStore::load(GameSettings& outSettings, std::string* outError) const
{
    outSettings = GameSettings{};
    std::ifstream file(path_, std::ios::binary);
    if (!file) {
        return true;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string text = buffer.str();
    if (text.size() >= 3 &&
        static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF) {
        text.erase(0, 3);
    }

    std::string parseError;
    JsonParser parser(text);
    std::optional<JsonValue> root = parser.parse(parseError);
    if (!root || root->type != JsonValue::Type::Object) {
        setError(outError, parseError.empty() ? "Settings root must be a JSON object" : parseError);
        return false;
    }

    GameSettings loaded;
    if (std::optional<int> version = intMember(*root, "version")) {
        loaded.version = *version;
    }

    if (const JsonValue* audio = objectMember(*root, "audio")) {
        if (std::optional<float> value = floatMember(*audio, "masterVolume")) {
            loaded.audio.masterVolume = *value;
        }
        if (std::optional<float> value = floatMember(*audio, "bgmVolume")) {
            loaded.audio.bgmVolume = *value;
        }
        if (std::optional<float> value = floatMember(*audio, "seVolume")) {
            loaded.audio.seVolume = *value;
        }
    }

    if (const JsonValue* video = objectMember(*root, "video")) {
        if (std::optional<std::string> value = stringMember(*video, "windowMode")) {
            WindowMode parsedMode = loaded.video.windowMode;
            if (parseWindowMode(*value, parsedMode)) {
                loaded.video.windowMode = parsedMode;
            }
        }
        if (std::optional<int> value = intMember(*video, "windowWidth")) {
            loaded.video.windowWidth = *value;
        }
        if (std::optional<int> value = intMember(*video, "windowHeight")) {
            loaded.video.windowHeight = *value;
        }
        if (std::optional<bool> value = boolMember(*video, "vsync")) {
            loaded.video.vsync = *value;
        }
    }

    if (const JsonValue* performance = objectMember(*root, "performance")) {
        if (std::optional<bool> value = boolMember(*performance, "lightweight")) {
            loaded.performance.lightweight = *value;
        }
    }

    if (const JsonValue* presentation = objectMember(*root, "presentation")) {
        if (std::optional<float> value = floatMember(*presentation, "brightness")) {
            loaded.presentation.brightness = *value;
        } else if (std::optional<std::string> value = stringMember(*presentation, "brightness")) {
            if (std::optional<float> parsed = parseScreenBrightnessValue(*value)) {
                loaded.presentation.brightness = *parsed;
            }
        }
        if (std::optional<std::string> value = stringMember(*presentation, "screenShake")) {
            ScreenShakeSetting parsed = loaded.presentation.screenShake;
            if (parseScreenShakeSetting(*value, parsed)) {
                loaded.presentation.screenShake = parsed;
            }
        }
        if (std::optional<std::string> value = stringMember(*presentation, "inputIcons")) {
            InputIconSetting parsed = loaded.presentation.inputIcons;
            if (parseInputIconSetting(*value, parsed)) {
                loaded.presentation.inputIcons = parsed;
            }
        }
    }

    if (const JsonValue* input = objectMember(*root, "input")) {
        loadInputBindings(*input, loaded.input.bindings);
    }

    migrateLoadedSettings(loaded);
    outSettings = sanitizeSettings(loaded);
    return true;
}

bool SettingsStore::save(const GameSettings& settings, std::string* outError) const
{
    const GameSettings sanitized = sanitizeSettings(settings);
    std::error_code ec;
    std::filesystem::create_directories(path_.parent_path(), ec);
    if (ec) {
        setError(outError, "Failed to create settings directory: " + ec.message());
        return false;
    }

    std::ofstream file(path_, std::ios::binary | std::ios::trunc);
    if (!file) {
        setError(outError, "Failed to open settings file: " + path_.string());
        return false;
    }

    file << "\xEF\xBB\xBF";
    file << "{\n";
    file << "  \"version\": " << sanitized.version << ",\n";
    file << "  \"audio\": {\n";
    file << std::fixed << std::setprecision(3);
    file << "    \"masterVolume\": " << sanitized.audio.masterVolume << ",\n";
    file << "    \"bgmVolume\": " << sanitized.audio.bgmVolume << ",\n";
    file << "    \"seVolume\": " << sanitized.audio.seVolume << "\n";
    file << "  },\n";
    file << "  \"video\": {\n";
    file << "    \"windowMode\": \"" << windowModeName(sanitized.video.windowMode) << "\",\n";
    file << "    \"windowWidth\": " << sanitized.video.windowWidth << ",\n";
    file << "    \"windowHeight\": " << sanitized.video.windowHeight << ",\n";
    file << "    \"vsync\": " << (sanitized.video.vsync ? "true" : "false") << "\n";
    file << "  },\n";
    file << "  \"performance\": {\n";
    file << "    \"lightweight\": " << (sanitized.performance.lightweight ? "true" : "false") << "\n";
    file << "  },\n";
    file << "  \"presentation\": {\n";
    file << "    \"brightness\": " << sanitized.presentation.brightness << ",\n";
    file << "    \"screenShake\": \"" << screenShakeSettingName(sanitized.presentation.screenShake) << "\",\n";
    file << "    \"inputIcons\": \"" << inputIconSettingName(sanitized.presentation.inputIcons) << "\"\n";
    file << "  },\n";
    file << "  \"input\": {\n";
    file << "    \"bindings\": {\n";
    bool firstAction = true;
    for (int action = 0; action < InputActionCount; ++action) {
        const auto inputAction = static_cast<InputAction>(action);
        if (!inputActionHasPersistentBindings(inputAction)) {
            continue;
        }
        if (!firstAction) {
            file << ",\n";
        }
        firstAction = false;
        file << "      \"" << inputActionName(inputAction) << "\": [";
        const std::vector<InputBinding>& bindings = sanitized.input.bindings[action];
        if (!bindings.empty()) {
            file << "\n";
            for (std::size_t i = 0; i < bindings.size(); ++i) {
                writeInputBindingJson(file, bindings[i], "        ");
                file << (i + 1 < bindings.size() ? ",\n" : "\n");
            }
            file << "      ]";
        } else {
            file << "]";
        }
    }
    file << "\n";
    file << "    }\n";
    file << "  }\n";
    file << "}\n";
    if (!file) {
        setError(outError, "Failed to write settings file: " + path_.string());
        return false;
    }

    return true;
}

}
