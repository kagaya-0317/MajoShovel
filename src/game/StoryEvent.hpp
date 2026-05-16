#pragma once

#include "game/DialogueSystem.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace majo {

struct StoryEvent {
    std::string id;
    std::string trigger;
    std::string onceFlag;
    DialogueSequence dialogue;
};

struct StoryEventLoadResult {
    std::vector<StoryEvent> events;
    std::vector<std::string> warnings;
};

class StoryEventLoader {
public:
    [[nodiscard]] StoryEventLoadResult loadDirectory(const std::filesystem::path& directory) const;
};

}
