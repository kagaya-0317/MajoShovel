#include "game/Kamishibai.hpp"

#include "engine/Log.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>

namespace majo {
namespace {

constexpr float DefaultPageDuration = 3.0f;
constexpr float CrossFadeSeconds = 0.8f;
constexpr float TextFadeSeconds = 0.45f;
constexpr int KamishibaiTextScale = 3;

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

std::string lowerAscii(std::string_view value)
{
    std::string lowered;
    lowered.reserve(value.size());
    for (char ch : value) {
        if (ch >= 'A' && ch <= 'Z') {
            lowered.push_back(static_cast<char>(ch - 'A' + 'a'));
        } else {
            lowered.push_back(ch);
        }
    }
    return lowered;
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

std::vector<std::string> splitTsvLine(std::string_view line)
{
    std::vector<std::string> columns;
    std::string current;
    for (char ch : line) {
        if (ch == '\t') {
            columns.push_back(std::move(current));
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    columns.push_back(std::move(current));
    return columns;
}

std::string textWithDisplayLineBreaks(std::string text)
{
    for (char& ch : text) {
        if (ch == '|') {
            ch = '\n';
        }
    }
    return text;
}

std::size_t utf8CodepointLength(unsigned char lead)
{
    if ((lead & 0x80U) == 0) {
        return 1;
    }
    if ((lead & 0xe0U) == 0xc0U) {
        return 2;
    }
    if ((lead & 0xf0U) == 0xe0U) {
        return 3;
    }
    if ((lead & 0xf8U) == 0xf0U) {
        return 4;
    }
    return 1;
}

std::string wrapTextForKamishibai(Renderer& renderer, std::string_view text, float maxWidth, int scale)
{
    if (maxWidth <= 0.0f || text.empty()) {
        return std::string(text);
    }

    std::string output;
    std::string line;
    for (std::size_t i = 0; i < text.size();) {
        const char ch = text[i];
        if (ch == '\n') {
            output += line;
            output.push_back('\n');
            line.clear();
            ++i;
            continue;
        }

        const std::size_t charLength = std::min(utf8CodepointLength(static_cast<unsigned char>(ch)), text.size() - i);
        const std::string_view token{text.data() + i, charLength};
        std::string candidate = line;
        candidate.append(token);
        if (!line.empty() && renderer.measureText(candidate, scale).x > maxWidth) {
            output += line;
            output.push_back('\n');
            line.assign(token);
        } else {
            line = std::move(candidate);
        }
        i += charLength;
    }

    output += line;
    return output;
}

std::vector<std::string_view> splitDisplayLines(std::string_view text)
{
    std::vector<std::string_view> lines;
    std::size_t lineBegin = 0;
    for (std::size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || text[i] == '\n') {
            lines.emplace_back(text.data() + lineBegin, i - lineBegin);
            lineBegin = i + 1;
        }
    }
    return lines;
}

void drawCenteredWrappedText(
    Renderer& renderer,
    Vec2 pos,
    std::string_view text,
    float maxWidth,
    Color color,
    int scale)
{
    const std::string wrapped = wrapTextForKamishibai(renderer, text, maxWidth, scale);
    const std::vector<std::string_view> lines = splitDisplayLines(wrapped);
    const float lineHeight = std::max(
        1.0f,
        renderer.measureText("M\nM", scale).y - renderer.measureText("M", scale).y);

    float y = pos.y;
    for (const std::string_view line : lines) {
        if (!line.empty()) {
            const Vec2 lineSize = renderer.measureText(line, scale);
            const float x = pos.x + (maxWidth - lineSize.x) * 0.5f;
            renderer.drawText({x, y}, line, color, scale);
        }
        y += lineHeight;
    }
}

bool parseDuration(std::string_view value, float& out)
{
    const std::string trimmed = trimAscii(value);
    if (trimmed.empty()) {
        return false;
    }
    char* end = nullptr;
    const float parsed = std::strtof(trimmed.c_str(), &end);
    if (end == trimmed.c_str() || (end != nullptr && *end != '\0') || !std::isfinite(parsed) || parsed <= 0.0f) {
        return false;
    }
    out = parsed;
    return true;
}

float smoothStep(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

unsigned char alphaByte(float alpha)
{
    return static_cast<unsigned char>(std::clamp(std::lround(alpha * 255.0f), 0L, 255L));
}

Color withAlpha(Color color, float alpha)
{
    color.a = alphaByte(alpha);
    return color;
}

Color multipliedAlpha(Color color, float alpha)
{
    const float baseAlpha = static_cast<float>(color.a) / 255.0f;
    color.a = alphaByte(baseAlpha * alpha);
    return color;
}

std::string pageLabel(const KamishibaiPage& page)
{
    return page.id.empty() ? std::string("(empty id)") : page.id;
}

KamishibaiPage fallbackPage()
{
    KamishibaiPage page;
    page.id = "fallback";
    page.imagePath = "assets/opening/op_8.png";
    page.text = "紙芝居データを読み込めませんでした。";
    page.duration = DefaultPageDuration;
    page.effect = KamishibaiEffect::None;
    page.effectName = "none";
    page.note = "fallback";
    return page;
}

bool validHeader(const std::vector<std::string>& columns)
{
    if (columns.size() < 6) {
        return false;
    }
    return trimAscii(columns[0]) == "id" &&
        trimAscii(columns[1]) == "image" &&
        trimAscii(columns[2]) == "text" &&
        trimAscii(columns[3]) == "duration" &&
        trimAscii(columns[4]) == "effect" &&
        trimAscii(columns[5]) == "note";
}

}

KamishibaiEffect kamishibaiEffectFromString(std::string_view value)
{
    const std::string normalized = lowerAscii(trimAscii(value));
    if (normalized == "flash") {
        return KamishibaiEffect::Flash;
    }
    if (normalized == "shake_dark") {
        return KamishibaiEffect::ShakeDark;
    }
    if (normalized == "title_fade") {
        return KamishibaiEffect::TitleFade;
    }
    return KamishibaiEffect::None;
}

std::string_view kamishibaiEffectName(KamishibaiEffect effect)
{
    switch (effect) {
    case KamishibaiEffect::None:
        return "none";
    case KamishibaiEffect::Flash:
        return "flash";
    case KamishibaiEffect::ShakeDark:
        return "shake_dark";
    case KamishibaiEffect::TitleFade:
        return "title_fade";
    }
    return "none";
}

KamishibaiLoadResult KamishibaiLoader::load(const std::filesystem::path& path) const
{
    KamishibaiLoadResult result;
    std::ifstream file(path);
    if (!file) {
        result.warnings.push_back("opening kamishibai TSV not found: " + path.generic_string());
        result.pages.push_back(fallbackPage());
        return result;
    }

    std::string line;
    if (!std::getline(file, line)) {
        result.warnings.push_back("opening kamishibai TSV is empty: " + path.generic_string());
        result.pages.push_back(fallbackPage());
        return result;
    }
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    stripUtf8Bom(line);
    const std::vector<std::string> header = splitTsvLine(line);
    if (!validHeader(header)) {
        result.warnings.push_back("opening kamishibai TSV header mismatch; expected id/image/text/duration/effect/note");
    }

    int lineNumber = 1;
    while (std::getline(file, line)) {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }

        std::vector<std::string> columns = splitTsvLine(line);
        if (columns.size() < 6) {
            result.warnings.push_back(
                "opening kamishibai TSV line " + std::to_string(lineNumber) +
                " has too few columns; missing columns were treated as empty");
            columns.resize(6);
        }
        if (columns.size() > 6) {
            result.warnings.push_back(
                "opening kamishibai TSV line " + std::to_string(lineNumber) +
                " has extra columns; extras were ignored");
        }

        KamishibaiPage page;
        page.id = trimAscii(columns[0]);
        page.imagePath = trimAscii(columns[1]);
        page.text = textWithDisplayLineBreaks(columns[2]);
        page.effectName = lowerAscii(trimAscii(columns[4]));
        page.effect = kamishibaiEffectFromString(page.effectName);
        page.note = columns[5];

        if (page.id.empty()) {
            page.id = "line_" + std::to_string(lineNumber);
            result.warnings.push_back("opening kamishibai TSV line " + std::to_string(lineNumber) + " has empty id");
        }
        if (!parseDuration(columns[3], page.duration)) {
            page.duration = DefaultPageDuration;
            result.warnings.push_back(
                "opening kamishibai page " + pageLabel(page) +
                " has invalid duration; using " + std::to_string(DefaultPageDuration));
        }
        if (page.effectName.empty()) {
            page.effectName = "none";
        }
        if (page.effect == KamishibaiEffect::None && page.effectName != "none") {
            result.warnings.push_back(
                "opening kamishibai page " + pageLabel(page) +
                " has unknown effect '" + page.effectName + "'; using none");
            page.effectName = "none";
        }
        if (!page.imagePath.empty() && !std::filesystem::exists(page.imagePath)) {
            result.warnings.push_back(
                "opening kamishibai image not found for " + pageLabel(page) + ": " + page.imagePath);
        }

        result.pages.push_back(std::move(page));
    }

    if (result.pages.empty()) {
        result.warnings.push_back("opening kamishibai TSV had no pages; using fallback page");
        result.pages.push_back(fallbackPage());
    }
    return result;
}

void KamishibaiPlayer::start(std::vector<KamishibaiPage> pages, bool canSkipImmediately)
{
    pages_ = std::move(pages);
    canSkipImmediately_ = canSkipImmediately;
    finished_ = pages_.empty();
    currentIndex_ = finished_ ? -1 : 0;
    previousIndex_ = -1;
    pageElapsed_ = 0.0f;
    transitionElapsed_ = CrossFadeSeconds;
    transitionActive_ = false;
}

void KamishibaiPlayer::update(float dt)
{
    if (finished_ || pages_.empty()) {
        return;
    }

    const float safeDt = std::max(0.0f, dt);
    pageElapsed_ += safeDt;
    if (transitionActive_) {
        transitionElapsed_ += safeDt;
        if (transitionElapsed_ >= CrossFadeSeconds) {
            transitionActive_ = false;
            previousIndex_ = -1;
            transitionElapsed_ = CrossFadeSeconds;
        }
    }

    const KamishibaiPage* page = currentPage();
    const float duration = page != nullptr ? std::max(0.05f, page->duration) : DefaultPageDuration;
    if (pageElapsed_ >= duration) {
        advancePage();
    }
}

void KamishibaiPlayer::finishImmediately()
{
    finished_ = true;
    previousIndex_ = -1;
    transitionActive_ = false;
}

float KamishibaiPlayer::pageProgress() const
{
    const KamishibaiPage* page = currentPage();
    if (page == nullptr || page->duration <= 0.0f) {
        return 1.0f;
    }
    return std::clamp(pageElapsed_ / page->duration, 0.0f, 1.0f);
}

float KamishibaiPlayer::transitionProgress() const
{
    if (!transitionActive_) {
        return 1.0f;
    }
    return std::clamp(transitionElapsed_ / CrossFadeSeconds, 0.0f, 1.0f);
}

const KamishibaiPage* KamishibaiPlayer::currentPage() const
{
    if (currentIndex_ < 0 || currentIndex_ >= static_cast<int>(pages_.size())) {
        return nullptr;
    }
    return &pages_[static_cast<std::size_t>(currentIndex_)];
}

const KamishibaiPage* KamishibaiPlayer::previousPage() const
{
    if (previousIndex_ < 0 || previousIndex_ >= static_cast<int>(pages_.size())) {
        return nullptr;
    }
    return &pages_[static_cast<std::size_t>(previousIndex_)];
}

void KamishibaiPlayer::advancePage()
{
    if (currentIndex_ + 1 >= static_cast<int>(pages_.size())) {
        finishImmediately();
        return;
    }

    previousIndex_ = currentIndex_;
    ++currentIndex_;
    pageElapsed_ = 0.0f;
    transitionElapsed_ = 0.0f;
    transitionActive_ = true;
}

void KamishibaiRenderer::render(Renderer& renderer, const KamishibaiPlayer& player, int screenWidth, int screenHeight) const
{
    const int width = std::max(1, screenWidth);
    const int height = std::max(1, screenHeight);
    renderer.setScreenSpace();
    renderer.fillRect({0.0f, 0.0f}, {static_cast<float>(width), static_cast<float>(height)}, {5, 5, 8, 255});

    const KamishibaiPage* current = player.currentPage();
    if (current == nullptr) {
        return;
    }

    Vec2 shakeOffset{};
    float overscan = 0.0f;
    if (current->effect == KamishibaiEffect::ShakeDark) {
        const float progress = player.pageProgress();
        const float strength = 5.0f * (1.0f - smoothStep(progress));
        shakeOffset = {
            std::sin(player.pageElapsed() * 34.0f) * strength,
            std::sin(player.pageElapsed() * 47.0f + 1.1f) * strength * 0.7f,
        };
        overscan = 12.0f;
    }

    const float fade = smoothStep(player.transitionProgress());
    if (const KamishibaiPage* previous = player.previousPage()) {
        drawCoverImage(renderer, *previous, width, height, 1.0f - fade, {}, 0.0f);
    }

    float currentAlpha = fade;
    if (current->effect == KamishibaiEffect::TitleFade) {
        currentAlpha *= smoothStep(player.pageElapsed() / 1.45f);
    }
    drawCoverImage(renderer, *current, width, height, currentAlpha, shakeOffset, overscan);

    if (current->effect == KamishibaiEffect::ShakeDark) {
        const float darkAlpha = 0.38f * smoothStep((player.pageProgress() - 0.42f) / 0.58f);
        renderer.fillRect(
            {0.0f, 0.0f},
            {static_cast<float>(width), static_cast<float>(height)},
            {0, 0, 0, alphaByte(darkAlpha)});
    }
    if (current->effect == KamishibaiEffect::Flash) {
        const float flash = std::max(0.0f, 1.0f - player.pageElapsed() / 0.55f);
        const float alpha = flash * flash * 0.92f;
        renderer.fillRect(
            {0.0f, 0.0f},
            {static_cast<float>(width), static_cast<float>(height)},
            {255, 255, 255, alphaByte(alpha)});
    }
    if (current->effect == KamishibaiEffect::TitleFade) {
        const float black = 0.42f * (1.0f - smoothStep(player.pageElapsed() / 1.8f));
        renderer.fillRect(
            {0.0f, 0.0f},
            {static_cast<float>(width), static_cast<float>(height)},
            {0, 0, 0, alphaByte(black)});
    }

    const float textDelay = player.currentIndex() == 0 ? 0.0f : CrossFadeSeconds;
    const float textAlpha = smoothStep((player.pageElapsed() - textDelay) / TextFadeSeconds);
    drawTextBand(renderer, current->text, width, height, textAlpha);
}

void KamishibaiRenderer::renderTitleScreen(Renderer& renderer, std::string_view imagePath, int screenWidth, int screenHeight) const
{
    const int width = std::max(1, screenWidth);
    const int height = std::max(1, screenHeight);
    KamishibaiPage page;
    page.id = "title";
    page.imagePath = std::string(imagePath);

    renderer.setScreenSpace();
    renderer.fillRect({0.0f, 0.0f}, {static_cast<float>(width), static_cast<float>(height)}, {5, 5, 8, 255});
    drawCoverImage(renderer, page, width, height, 1.0f, {}, 0.0f);
    renderer.fillGradientRect(
        {0.0f, 0.0f},
        {static_cast<float>(width), static_cast<float>(height)},
        {0, 0, 0, 120},
        {0, 0, 0, 38},
        {0, 0, 0, 150},
        {0, 0, 0, 92});

    const std::string title = "魔女採掘";
    const int titleScale = width >= 1000 ? 7 : 5;
    const Vec2 titleSize = renderer.measureText(title, titleScale);
    const Vec2 titlePos{
        (static_cast<float>(width) - titleSize.x) * 0.5f,
        static_cast<float>(height) * 0.35f - titleSize.y * 0.5f,
    };
    renderer.drawOutlinedText(titlePos, title, {255, 248, 220, 255}, {0, 0, 0, 185}, 8, titleScale);

    const std::string prompt = "Enter / F / Click";
    const int promptScale = width >= 800 ? 3 : 2;
    const Vec2 promptSize = renderer.measureText(prompt, promptScale);
    const Vec2 promptPos{
        (static_cast<float>(width) - promptSize.x) * 0.5f,
        static_cast<float>(height) * 0.76f,
    };
    renderer.drawOutlinedText(promptPos, prompt, {246, 238, 214, 235}, {0, 0, 0, 170}, 5, promptScale);
}

void KamishibaiRenderer::drawCoverImage(
    Renderer& renderer,
    const KamishibaiPage& page,
    int screenWidth,
    int screenHeight,
    float alpha,
    Vec2 offset,
    float overscan) const
{
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    if (alpha <= 0.0f) {
        return;
    }

    const float width = static_cast<float>(std::max(1, screenWidth));
    const float height = static_cast<float>(std::max(1, screenHeight));
    const float drawWidth = width + overscan * 2.0f;
    const float drawHeight = height + overscan * 2.0f;

    bool drewImage = false;
    if (!page.imagePath.empty()) {
        const ImageHandle handle = renderer.acquireImage(page.imagePath, TextureFilter::Linear);
        Vec2 imageSize{};
        if (handle.valid() && renderer.getImageSize(handle, imageSize) && imageSize.x > 0.0f && imageSize.y > 0.0f) {
            const float imageAspect = imageSize.x / imageSize.y;
            const float targetAspect = drawWidth / drawHeight;
            SDL_FRect source{0.0f, 0.0f, imageSize.x, imageSize.y};
            if (imageAspect > targetAspect) {
                source.w = imageSize.y * targetAspect;
                source.x = (imageSize.x - source.w) * 0.5f;
            } else {
                source.h = imageSize.x / targetAspect;
                source.y = (imageSize.y - source.h) * 0.5f;
            }

            ImageDrawOptions options;
            options.tint = {255, 255, 255, alphaByte(alpha)};
            drewImage = renderer.drawImageRegion(
                handle,
                source,
                {width * 0.5f + offset.x, height * 0.5f + offset.y},
                {drawWidth, drawHeight},
                options);
        }
    }

    if (drewImage) {
        return;
    }

    renderer.fillGradientRect(
        {0.0f, 0.0f},
        {width, height},
        withAlpha({24, 28, 44, 255}, alpha),
        withAlpha({62, 54, 78, 255}, alpha),
        GradientDirection::TopToBottom);
    renderer.fillCircle({width * 0.50f, height * 0.42f}, std::min(width, height) * 0.18f, withAlpha({146, 128, 86, 120}, alpha));
    renderer.drawCircle({width * 0.50f, height * 0.42f}, std::min(width, height) * 0.18f, withAlpha({238, 218, 154, 170}, alpha));
    if (!page.id.empty()) {
        const int scale = width >= 800.0f ? 3 : 2;
        const Vec2 labelSize = renderer.measureText(page.id, scale);
        renderer.drawOutlinedText(
            {width * 0.5f - labelSize.x * 0.5f, height * 0.48f},
            page.id,
            withAlpha({238, 232, 214, 255}, alpha),
            withAlpha({0, 0, 0, 190}, alpha),
            5,
            scale);
    }
}

void KamishibaiRenderer::drawTextBand(
    Renderer& renderer,
    std::string_view text,
    int screenWidth,
    int screenHeight,
    float alpha) const
{
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    if (text.empty() || alpha <= 0.0f) {
        return;
    }

    const float width = static_cast<float>(std::max(1, screenWidth));
    const float height = static_cast<float>(std::max(1, screenHeight));
    const float bandHeight = std::clamp(height * 0.28f, 150.0f, 230.0f);
    const float bandY = height - bandHeight;

    const float paddingX = std::clamp(width * 0.085f, 42.0f, 110.0f);
    const float paddingY = std::clamp(bandHeight * 0.18f, 22.0f, 42.0f);
    const float textWidth = std::max(1.0f, width - paddingX * 2.0f);
    constexpr int scale = KamishibaiTextScale;
    const Vec2 textSize = renderer.measureWrappedText(text, textWidth, scale);
    const Vec2 pos{
        paddingX,
        bandY + std::max(paddingY, (bandHeight - textSize.y) * 0.5f),
    };
    drawCenteredWrappedText(
        renderer,
        pos + Vec2{2.0f, 2.0f},
        text,
        textWidth,
        multipliedAlpha({0, 0, 0, 210}, alpha),
        scale);
    drawCenteredWrappedText(
        renderer,
        pos,
        text,
        textWidth,
        multipliedAlpha({255, 255, 255, 245}, alpha),
        scale);
}

}
