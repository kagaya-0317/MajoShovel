#pragma once

#include "engine/RendererTypes.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace majo {

struct PortraitSpeakerDefinition {
    std::string_view speakerId;
    std::string_view displayName;
    int baseNumber = 0;
    Color fallbackColor{156, 168, 184, 255};
};

struct PortraitVariant {
    int index = 0;
    std::string path;
    std::string label;
    bool fallback = false;
};

[[nodiscard]] const PortraitSpeakerDefinition* portraitSpeakerDefinition(std::string_view speakerId);
[[nodiscard]] std::string portraitSpeakerDisplayName(std::string_view speakerId);
[[nodiscard]] Color portraitSpeakerFallbackColor(std::string_view speakerId);
[[nodiscard]] bool portraitSpeakerHasPortrait(std::string_view speakerId);
[[nodiscard]] std::string portraitBasePath(std::string_view speakerId);
[[nodiscard]] std::vector<PortraitVariant> portraitVariantsForSpeaker(std::string_view speakerId);
[[nodiscard]] int defaultPortraitVariant(std::string_view speakerId);
[[nodiscard]] std::string portraitPathForSpeaker(std::string_view speakerId, int variantIndex);
[[nodiscard]] bool portraitPathUsesScaledSource(std::string_view path);
[[nodiscard]] RectF portraitFaceSourceRect(Vec2 sourceSize);

} // namespace majo
