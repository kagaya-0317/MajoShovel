#include "game/PortraitCatalog.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace majo {
namespace {

constexpr std::string_view PortraitRoot = "assets/taties";
constexpr std::array<PortraitSpeakerDefinition, 6> PortraitSpeakers{{
    {"player", "ルネ", 1, {132, 86, 178, 255}},
    {"chicory", "チコリ", 2, {230, 212, 112, 255}},
    {"monica", "モニカ", 3, {88, 128, 214, 255}},
    {"elder", "村長", 4, {126, 154, 116, 255}},
    {"merchant", "商人", 5, {196, 134, 86, 255}},
    {"processor", "加工職人", 6, {108, 148, 166, 255}},
}};

[[nodiscard]] std::string portraitFileStem(int baseNumber)
{
    return "tatie_" + std::to_string(baseNumber);
}

[[nodiscard]] std::string portraitBasePathForNumber(int baseNumber)
{
    return (std::filesystem::path(std::string(PortraitRoot)) / (portraitFileStem(baseNumber) + ".png")).generic_string();
}

[[nodiscard]] bool decimalInteger(std::string_view value)
{
    return !value.empty() &&
        std::all_of(value.begin(), value.end(), [](char c) {
            return std::isdigit(static_cast<unsigned char>(c)) != 0;
        });
}

[[nodiscard]] std::string variantLabel(int index, bool fallback)
{
    if (fallback) {
        return "標準";
    }
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "#%02d", std::max(0, index));
    return buffer;
}

struct PortraitVariantCache {
    std::filesystem::file_time_type rootWriteTime{};
    bool rootWriteTimeValid = false;
    std::array<std::vector<PortraitVariant>, PortraitSpeakers.size()> variants;
};

PortraitVariantCache& portraitVariantCache()
{
    static PortraitVariantCache cache;
    return cache;
}

std::size_t portraitSpeakerIndex(const PortraitSpeakerDefinition& definition)
{
    return static_cast<std::size_t>(&definition - PortraitSpeakers.data());
}

bool portraitRootChanged(PortraitVariantCache& cache, const std::filesystem::path& root)
{
    std::error_code ec;
    const std::filesystem::file_time_type writeTime = std::filesystem::last_write_time(root, ec);
    const bool valid = !ec;
    if (valid != cache.rootWriteTimeValid || (valid && writeTime != cache.rootWriteTime)) {
        cache.rootWriteTime = writeTime;
        cache.rootWriteTimeValid = valid;
        return true;
    }
    return false;
}

} // namespace

const PortraitSpeakerDefinition* portraitSpeakerDefinition(std::string_view speakerId)
{
    const auto it = std::find_if(PortraitSpeakers.begin(), PortraitSpeakers.end(), [speakerId](const PortraitSpeakerDefinition& definition) {
        return definition.speakerId == speakerId;
    });
    return it == PortraitSpeakers.end() ? nullptr : &*it;
}

std::string portraitSpeakerDisplayName(std::string_view speakerId)
{
    const PortraitSpeakerDefinition* definition = portraitSpeakerDefinition(speakerId);
    return definition != nullptr ? std::string(definition->displayName) : std::string(speakerId);
}

Color portraitSpeakerFallbackColor(std::string_view speakerId)
{
    const PortraitSpeakerDefinition* definition = portraitSpeakerDefinition(speakerId);
    return definition != nullptr ? definition->fallbackColor : Color{156, 168, 184, 255};
}

bool portraitSpeakerHasPortrait(std::string_view speakerId)
{
    return portraitSpeakerDefinition(speakerId) != nullptr;
}

std::string portraitBasePath(std::string_view speakerId)
{
    const PortraitSpeakerDefinition* definition = portraitSpeakerDefinition(speakerId);
    return definition != nullptr ? portraitBasePathForNumber(definition->baseNumber) : std::string{};
}

std::vector<PortraitVariant> portraitVariantsForSpeaker(std::string_view speakerId)
{
    const PortraitSpeakerDefinition* definition = portraitSpeakerDefinition(speakerId);
    if (definition == nullptr || definition->baseNumber <= 0) {
        return {};
    }

    const std::filesystem::path root{std::string(PortraitRoot)};
    PortraitVariantCache& cache = portraitVariantCache();
    const bool rootChanged = portraitRootChanged(cache, root);
    if (rootChanged) {
        for (std::vector<PortraitVariant>& cachedVariants : cache.variants) {
            cachedVariants.clear();
        }
    }
    const std::size_t speakerIndex = portraitSpeakerIndex(*definition);
    if (!rootChanged && !cache.variants[speakerIndex].empty()) {
        return cache.variants[speakerIndex];
    }

    std::vector<PortraitVariant> variants;
    const std::string prefix = portraitFileStem(definition->baseNumber) + "_";
    std::error_code ec;
    if (std::filesystem::exists(root, ec)) {
        for (std::filesystem::directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied, ec), end;
            !ec && it != end;
            it.increment(ec)) {
            if (!it->is_regular_file(ec) || it->path().extension() != ".png") {
                continue;
            }
            const std::string stem = it->path().stem().generic_string();
            if (stem.rfind(prefix, 0) != 0) {
                continue;
            }
            const std::string suffix = stem.substr(prefix.size());
            if (!decimalInteger(suffix)) {
                continue;
            }
            const int index = std::max(0, std::atoi(suffix.c_str()));
            if (index <= 0) {
                continue;
            }
            variants.push_back(PortraitVariant{
                index,
                it->path().generic_string(),
                variantLabel(index, false),
                false,
            });
        }
    }

    std::sort(variants.begin(), variants.end(), [](const PortraitVariant& left, const PortraitVariant& right) {
        return left.index < right.index;
    });
    variants.erase(
        std::unique(variants.begin(), variants.end(), [](const PortraitVariant& left, const PortraitVariant& right) {
            return left.index == right.index;
        }),
        variants.end());

    if (variants.empty()) {
        variants.push_back(PortraitVariant{
            0,
            portraitBasePathForNumber(definition->baseNumber),
            variantLabel(0, true),
            true,
        });
    }
    cache.variants[speakerIndex] = variants;
    return cache.variants[speakerIndex];
}

int defaultPortraitVariant(std::string_view speakerId)
{
    const std::vector<PortraitVariant> variants = portraitVariantsForSpeaker(speakerId);
    return variants.empty() ? 0 : variants.front().index;
}

std::string portraitPathForSpeaker(std::string_view speakerId, int variantIndex)
{
    const std::vector<PortraitVariant> variants = portraitVariantsForSpeaker(speakerId);
    if (variants.empty()) {
        return {};
    }
    const auto it = std::find_if(variants.begin(), variants.end(), [variantIndex](const PortraitVariant& variant) {
        return variant.index == variantIndex;
    });
    return (it != variants.end() ? it : variants.begin())->path;
}

bool portraitPathUsesScaledSource(std::string_view path)
{
    const std::filesystem::path parsed{std::string(path)};
    const std::string stem = parsed.stem().generic_string();
    const std::size_t underscore = stem.rfind('_');
    const std::size_t firstUnderscore = stem.find('_');
    if (firstUnderscore == std::string::npos ||
        stem.find('_', firstUnderscore + 1) == std::string::npos ||
        underscore == std::string::npos ||
        underscore + 1 >= stem.size()) {
        return false;
    }
    const std::string_view suffix(stem.data() + underscore + 1, stem.size() - underscore - 1);
    return decimalInteger(suffix) && stem.rfind("tatie_", 0) == 0;
}

RectF portraitFaceSourceRect(Vec2 sourceSize)
{
    const float width = std::max(1.0f, sourceSize.x);
    const float height = std::max(1.0f, sourceSize.y);
    constexpr float SourceZoom = 1.5f;
    constexpr float SourceWidth = 0.50f / SourceZoom;
    constexpr float SourceHeight = 0.34f / SourceZoom;
    constexpr float SourceCenterY = 0.24f;
    return {
        width * ((1.0f - SourceWidth) * 0.5f),
        height * (SourceCenterY - SourceHeight * 0.5f),
        width * SourceWidth,
        height * SourceHeight,
    };
}

} // namespace majo
