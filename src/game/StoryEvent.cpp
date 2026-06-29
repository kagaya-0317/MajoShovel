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

constexpr float BaseHintAutoPortraitHideSeconds = 0.22f;

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
    bool forcePortraitsBright = false;
    std::vector<std::string> brightPortraitSpeakerIds;
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

float portraitHideSecondsFor(std::string_view value)
{
    if (trimAscii(value).empty()) {
        return 0.18f;
    }
    return waitSecondsFor(value);
}

std::vector<std::string> splitAsciiTokens(std::string_view value)
{
    std::vector<std::string> tokens;
    std::string remaining = trimAscii(value);
    while (!remaining.empty()) {
        const std::size_t space = remaining.find_first_of(" \t");
        if (space == std::string::npos) {
            tokens.push_back(remaining);
            break;
        }
        tokens.push_back(remaining.substr(0, space));
        remaining = trimAscii(std::string_view(remaining).substr(space + 1));
    }
    return tokens;
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
    line.forcePortraitsBright = block.forcePortraitsBright;
    line.brightPortraitSpeakerIds = block.brightPortraitSpeakerIds;

    event.dialogue.lines.push_back(line);
    DialogueStep step;
    step.kind = DialogueStepKind::Line;
    step.line = std::move(line);
    event.dialogue.steps.push_back(std::move(step));
    block = TextBlock{};
}

void appendWaitToEvent(StoryEvent& event, float seconds)
{
    DialogueStep step;
    step.kind = DialogueStepKind::Wait;
    step.waitSeconds = seconds;
    event.dialogue.steps.push_back(std::move(step));
}

void appendPortraitHideToEvent(StoryEvent& event, float seconds)
{
    DialogueStep step;
    step.kind = DialogueStepKind::PortraitHide;
    step.waitSeconds = seconds;
    event.dialogue.steps.push_back(std::move(step));
}

void appendPortraitHideSpeakerToEvent(StoryEvent& event, std::string speakerId, float seconds)
{
    DialogueStep step;
    step.kind = DialogueStepKind::PortraitHideSpeaker;
    step.portraitSpeakerId = std::move(speakerId);
    step.waitSeconds = seconds;
    event.dialogue.steps.push_back(std::move(step));
}

void appendCommandToEvent(StoryEvent& event, std::string command, std::string_view rest)
{
    DialogueStep step;
    step.kind = DialogueStepKind::Command;
    step.command.name = std::move(command);
    step.command.args = splitAsciiTokens(rest);

    event.dialogue.steps.push_back(std::move(step));
}

bool storyCommandArgIsOff(std::string_view value)
{
    return value == "hide" || value == "off" || value == "clear";
}

bool isBaseHintNotification(const StoryEvent& event)
{
    return event.id.rfind("base_hint_", 0) == 0 || event.trigger.rfind("base_hint:", 0) == 0;
}

bool isBaseFacilityMarkerShowStep(const DialogueStep& step)
{
    if (step.kind != DialogueStepKind::Command || step.command.name != "base_facility_marker") {
        return false;
    }
    if (step.command.args.empty() || step.command.args[0] == "clear") {
        return false;
    }
    return step.command.args.size() < 2 || !storyCommandArgIsOff(step.command.args[1]);
}

void applyBaseHintNotificationConventions(StoryEvent& event)
{
    if (!isBaseHintNotification(event)) {
        return;
    }

    std::vector<DialogueStep> steps;
    steps.reserve(event.dialogue.steps.size() + 1);
    bool portraitHideInserted = false;
    for (DialogueStep& step : event.dialogue.steps) {
        if (!portraitHideInserted && isBaseFacilityMarkerShowStep(step)) {
            if (steps.empty() || steps.back().kind != DialogueStepKind::PortraitHide) {
                DialogueStep hideStep;
                hideStep.kind = DialogueStepKind::PortraitHide;
                hideStep.waitSeconds = BaseHintAutoPortraitHideSeconds;
                hideStep.persistPortraitHide = true;
                steps.push_back(std::move(hideStep));
            } else {
                steps.back().persistPortraitHide = true;
            }
            portraitHideInserted = true;
        }
        steps.push_back(std::move(step));
    }
    event.dialogue.steps = std::move(steps);
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
    bool forceNextPortraitsBright = false;
    std::vector<std::string> nextBrightPortraitSpeakerIds;

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
        } else if (command == "presentation") {
            event.presentation = rest;
        } else if (command == "trigger") {
            event.trigger = rest;
        } else if (command == "once") {
            event.onceFlag = rest;
            event.repeatable = false;
        } else if (command == "repeat") {
            event.onceFlag.clear();
            event.repeatable = true;
        } else if (command == "debug") {
            if (rest == "hidden") {
                event.debugHidden = true;
            } else {
                warnings.push_back(
                    "story event " + path.generic_string() + ":" + std::to_string(lineNumber) +
                    " unknown debug option " + rest);
            }
        } else if (command == "narration") {
            block.kind = TextBlockKind::Narration;
            block.forcePortraitsBright = forceNextPortraitsBright;
            block.brightPortraitSpeakerIds = std::move(nextBrightPortraitSpeakerIds);
            forceNextPortraitsBright = false;
            nextBrightPortraitSpeakerIds.clear();
        } else if (command == "say") {
            const auto [speakerId, speakerName] = splitFirstToken(rest);
            block.kind = TextBlockKind::Say;
            block.speakerId = speakerId;
            block.speakerName = speakerName;
            block.forcePortraitsBright = forceNextPortraitsBright;
            block.brightPortraitSpeakerIds = std::move(nextBrightPortraitSpeakerIds);
            forceNextPortraitsBright = false;
            nextBrightPortraitSpeakerIds.clear();
        } else if (command == "wait") {
            appendWaitToEvent(event, waitSecondsFor(rest));
        } else if (command == "portraits_hide") {
            appendPortraitHideToEvent(event, portraitHideSecondsFor(rest));
        } else if (command == "portrait_hide") {
            std::vector<std::string> args = splitAsciiTokens(rest);
            if (args.empty()) {
                warnings.push_back(
                    "story event " + path.generic_string() + ":" + std::to_string(lineNumber) +
                    " missing speaker id for @portrait_hide");
            } else {
                const float seconds = args.size() >= 2
                    ? portraitHideSecondsFor(args[1])
                    : portraitHideSecondsFor({});
                appendPortraitHideSpeakerToEvent(event, std::move(args[0]), seconds);
            }
        } else if (command == "portraits_bright") {
            forceNextPortraitsBright = true;
            nextBrightPortraitSpeakerIds.clear();
        } else if (command == "portrait_focus") {
            forceNextPortraitsBright = false;
            nextBrightPortraitSpeakerIds = splitAsciiTokens(rest);
        } else if (
            command.rfind("base_", 0) == 0 ||
            command.rfind("dungeon_", 0) == 0 ||
            command == "story_shake" ||
            command == "story_phone" ||
            command == "story_jingle") {
            appendCommandToEvent(event, command, rest);
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
    applyBaseHintNotificationConventions(event);
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
