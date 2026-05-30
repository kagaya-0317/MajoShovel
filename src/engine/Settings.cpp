#include "engine/Settings.hpp"

#include <algorithm>
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

constexpr int CurrentSettingsVersion = 1;
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

void writeInputBindingJson(std::ostream& out, const InputBinding& binding, std::string_view indent)
{
    out << indent << "{ \"device\": \"" << inputBindingDeviceName(binding.device) << "\"";
    switch (binding.device) {
    case InputBindingDevice::Keyboard:
        out << ", \"key\": \"" << jsonEscape(keyboardScancodeName(binding.code)) << "\"";
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

GameSettings sanitizeSettings(GameSettings settings)
{
    settings.version = CurrentSettingsVersion;
    settings.audio.masterVolume = clampVolume(settings.audio.masterVolume);
    settings.audio.bgmVolume = clampVolume(settings.audio.bgmVolume);
    settings.audio.seVolume = clampVolume(settings.audio.seVolume);
    settings.video.windowWidth = std::clamp(settings.video.windowWidth, MinWindowWidth, MaxWindowWidth);
    settings.video.windowHeight = std::clamp(settings.video.windowHeight, MinWindowHeight, MaxWindowHeight);
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

    if (const JsonValue* input = objectMember(*root, "input")) {
        loadInputBindings(*input, loaded.input.bindings);
    }

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
    file << "  \"input\": {\n";
    file << "    \"bindings\": {\n";
    for (int action = 0; action < InputActionCount; ++action) {
        const auto inputAction = static_cast<InputAction>(action);
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
        file << (action + 1 < InputActionCount ? ",\n" : "\n");
    }
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
