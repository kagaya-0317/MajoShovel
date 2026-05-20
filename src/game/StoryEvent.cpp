#include "game/StoryEvent.hpp"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <string_view>
#include <system_error>
#include <utility>

namespace majo {
namespace {

enum class TextBlockKind {
    None,
    Narration,
    Say,
};

struct TextBlock {
    TextBlockKind kind = TextBlockKind::None;
    std::string speakerId;
    std::string speakerName;
    std::vector<std::string> lines;
};

std::string trimAscii(std::string_view value)
{
    std::size_t begin = 0;
    while (begin < value.size() && static_cast<unsigned char>(value[begin]) <= 0x20U) {
        ++begin;
    }
    std::size_t end = value.size();
    while (end > begin && static_cast<unsigned char>(value[end - 1]) <= 0x20U) {
        --end;
    }
    return std::string(value.substr(begin, end - begin));
}

void stripUtf8Bom(std::string& line)
{
    if (line.size() >= 3 &&
        static_cast<unsigned char>(line[0]) == 0xefU &&
        static_cast<unsigned char>(line[1]) == 0xbbU &&
        static_cast<unsigned char>(line[2]) == 0xbfU) {
        line.erase(0, 3);
    }
}

std::string joinTextLines(const std::vector<std::string>& lines)
{
    std::string text;
    for (const std::string& line : lines) {
        if (!text.empty()) {
            text += '\n';
        }
        text += line;
    }
    return text;
}

float waitSecondsFor(std::string_view value)
{
    const std::string normalized = trimAscii(value);
    if (normalized == "small") {
        return 0.45f;
    }
    if (normalized == "medium") {
        return 0.8f;
    }
    if (normalized == "long") {
        return 1.2f;
    }

    errno = 0;
    char* end = nullptr;
    const float parsed = std::strtof(normalized.c_str(), &end);
    if (end != normalized.c_str() && end != nullptr && *end == '\0' && errno == 0 && std::isfinite(parsed)) {
        return std::max(0.0f, parsed);
    }
    return 0.45f;
}

void appendBlockToEvent(StoryEvent& event, TextBlock& block)
{
    if (block.kind == TextBlockKind::None) {
        return;
    }

    DialogueLine line;
    line.text = joinTextLines(block.lines);
    if (block.kind == TextBlockKind::Say) {
        line.speakerId = std::move(block.speakerId);
        line.speakerName = std::move(block.speakerName);
    }

    event.dialogue.lines.push_back(line);
    event.dialogue.steps.push_back(DialogueStep{DialogueStepKind::Line, std::move(line), 0.0f});
    block = TextBlock{};
}

void appendWaitToEvent(StoryEvent& event, float seconds)
{
    DialogueStep step;
    step.kind = DialogueStepKind::Wait;
    step.waitSeconds = seconds;
    event.dialogue.steps.push_back(std::move(step));
}

std::pair<std::string, std::string> splitFirstToken(std::string_view value)
{
    const std::string trimmed = trimAscii(value);
    const std::size_t space = trimmed.find_first_of(" \t");
    if (space == std::string::npos) {
        return {trimmed, {}};
    }
    return {
        trimmed.substr(0, space),
        trimAscii(std::string_view(trimmed).substr(space + 1)),
    };
}

void loadStoryEventFile(
    const std::filesystem::path& path,
    std::vector<StoryEvent>& events,
    std::vector<std::string>& warnings)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        warnings.push_back("story event not found: " + path.generic_string());
        return;
    }

    StoryEvent event;
    event.id = path.stem().generic_string();
    TextBlock block;

    std::string line;
    bool firstLine = true;
    int lineNumber = 0;
    while (std::getline(file, line)) {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (firstLine) {
            stripUtf8Bom(line);
            firstLine = false;
        }

        const std::string trimmed = trimAscii(line);
        if (trimmed.empty()) {
            continue;
        }
        if (trimmed[0] == '#') {
            continue;
        }
        if (trimmed[0] != '@') {
            if (block.kind != TextBlockKind::None) {
                block.lines.push_back(line);
            } else {
                warnings.push_back(
                    "story event " + path.generic_string() + ":" + std::to_string(lineNumber) +
                    " ignored text outside a block");
            }
            continue;
        }

        appendBlockToEvent(event, block);

        const auto [command, rest] = splitFirstToken(std::string_view(trimmed).substr(1));
        if (command == "event") {
            event.id = rest.empty() ? event.id : rest;
            event.dialogue.id = event.id;
        } else if (command == "title") {
            event.title = rest;
        } else if (command == "trigger") {
            event.trigger = rest;
        } else if (command == "once") {
            event.onceFlag = rest;
            event.repeatable = false;
        } else if (command == "repeat") {
            event.onceFlag.clear();
            event.repeatable = true;
        } else if (command == "narration") {
            block.kind = TextBlockKind::Narration;
        } else if (command == "say") {
            const auto [speakerId, speakerName] = splitFirstToken(rest);
            block.kind = TextBlockKind::Say;
            block.speakerId = speakerId;
            block.speakerName = speakerName;
        } else if (command == "wait") {
            appendWaitToEvent(event, waitSecondsFor(rest));
        } else {
            warnings.push_back(
                "story event " + path.generic_string() + ":" + std::to_string(lineNumber) +
                " unknown command @" + command);
        }
    }

    appendBlockToEvent(event, block);

    if (event.dialogue.id.empty()) {
        event.dialogue.id = event.id;
    }
    if (!event.repeatable && event.onceFlag.empty() && !event.id.empty()) {
        event.onceFlag = "story_" + event.id;
    }
    if (event.dialogue.steps.empty()) {
        warnings.push_back("story event " + path.generic_string() + " has no steps");
        return;
    }
    events.push_back(std::move(event));
}

}

StoryEventLoadResult StoryEventLoader::loadDirectory(const std::filesystem::path& directory) const
{
    StoryEventLoadResult result;
    std::error_code ec;
    if (!std::filesystem::exists(directory, ec)) {
        result.warnings.push_back("story event directory not found: " + directory.generic_string());
        return result;
    }

    std::vector<std::filesystem::path> files;
    for (std::filesystem::directory_iterator it(directory, std::filesystem::directory_options::skip_permission_denied, ec), end;
        !ec && it != end;
        it.increment(ec)) {
        if (it->is_regular_file(ec) && it->path().extension() == ".story") {
            files.push_back(it->path());
        }
    }
    std::sort(files.begin(), files.end());

    for (const std::filesystem::path& file : files) {
        loadStoryEventFile(file, result.events, result.warnings);
    }
    return result;
}

}
