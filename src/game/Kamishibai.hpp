#pragma once

#include "engine/Math.hpp"
#include "engine/Renderer.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace majo {

enum class KamishibaiEffect {
    None,
    Flash,
    ShakeDark,
    TitleFade,
};

struct KamishibaiPage {
    std::string id;
    std::string imagePath;
    std::string text;
    float duration = 3.0f;
    KamishibaiEffect effect = KamishibaiEffect::None;
    std::string effectName = "none";
    std::string note;
};

struct KamishibaiLoadResult {
    std::vector<KamishibaiPage> pages;
    std::vector<std::string> warnings;
};

class KamishibaiLoader {
public:
    [[nodiscard]] KamishibaiLoadResult load(const std::filesystem::path& path) const;
};

class KamishibaiPlayer {
public:
    void start(std::vector<KamishibaiPage> pages, bool canSkipImmediately);
    void update(float dt);
    void finishImmediately();

    [[nodiscard]] bool finished() const { return finished_; }
    [[nodiscard]] bool canSkipImmediately() const { return canSkipImmediately_; }
    [[nodiscard]] bool hasPages() const { return !pages_.empty(); }
    [[nodiscard]] int currentIndex() const { return currentIndex_; }
    [[nodiscard]] int previousIndex() const { return previousIndex_; }
    [[nodiscard]] float pageElapsed() const { return pageElapsed_; }
    [[nodiscard]] float pageProgress() const;
    [[nodiscard]] float transitionProgress() const;
    [[nodiscard]] const KamishibaiPage* currentPage() const;
    [[nodiscard]] const KamishibaiPage* previousPage() const;

private:
    void advancePage();

    std::vector<KamishibaiPage> pages_;
    int currentIndex_ = -1;
    int previousIndex_ = -1;
    float pageElapsed_ = 0.0f;
    float transitionElapsed_ = 0.0f;
    bool transitionActive_ = false;
    bool finished_ = true;
    bool canSkipImmediately_ = false;
};

class KamishibaiRenderer {
public:
    void render(Renderer& renderer, const KamishibaiPlayer& player, int screenWidth, int screenHeight) const;
    void renderTitleScreen(Renderer& renderer, std::string_view imagePath, int screenWidth, int screenHeight) const;

private:
    void drawCoverImage(
        Renderer& renderer,
        const KamishibaiPage& page,
        int screenWidth,
        int screenHeight,
        float alpha,
        Vec2 offset,
        float overscan) const;
    void drawTextBand(
        Renderer& renderer,
        std::string_view text,
        int screenWidth,
        int screenHeight,
        float alpha) const;
};

[[nodiscard]] KamishibaiEffect kamishibaiEffectFromString(std::string_view value);
[[nodiscard]] std::string_view kamishibaiEffectName(KamishibaiEffect effect);

}
