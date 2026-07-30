#include "game/Kamishibai.hpp"

#include "engine/Log.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>

namespace majo {
namespace {

constexpr float DefaultPageDuration = 3.0f;
constexpr float CrossFadeSeconds = 0.8f;
constexpr float OverlayFadeSeconds = 0.28f;
constexpr float TitleFromBlackFadeInSeconds = 1.45f;
constexpr float FlashWhiteoutFlashSeconds = 0.55f;
constexpr float FlashWhiteoutHoldSeconds = 0.25f;
constexpr float TextAppearDelaySeconds = 0.5f;
constexpr float TextFadeSeconds = 0.45f;
constexpr int KamishibaiTextScale = 3;
constexpr std::string_view TextStepSeparator = "[[next]]";

struct KamishibaiTextLineLayout {
    std::string text;
    Vec2 offset;
};

struct KamishibaiTextShadowLayer {
    Vec2 offset;
    Color color;
};

Color multipliedAlpha(Color color, float alpha);

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

std::vector<std::string> splitTextSteps(std::string_view text)
{
    std::vector<std::string> steps;
    std::size_t begin = 0;
    while (begin <= text.size()) {
        const std::size_t separator = text.find(TextStepSeparator, begin);
        const std::size_t end = separator == std::string_view::npos ? text.size() : separator;
        steps.push_back(textWithDisplayLineBreaks(std::string(text.substr(begin, end - begin))));
        if (separator == std::string_view::npos) {
            break;
        }
        begin = separator + TextStepSeparator.size();
    }
    if (steps.empty()) {
        steps.push_back({});
    }
    return steps;
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

std::vector<KamishibaiTextLineLayout> layoutCenteredWrappedText(
    Renderer& renderer,
    std::string_view text,
    float maxWidth,
    int scale)
{
    const std::string wrapped = wrapTextForKamishibai(renderer, text, maxWidth, scale);
    const std::vector<std::string_view> lines = splitDisplayLines(wrapped);
    const float lineHeight = std::max(
        1.0f,
        renderer.measureText("M\nM", scale).y - renderer.measureText("M", scale).y);

    std::vector<KamishibaiTextLineLayout> layout;
    layout.reserve(lines.size());
    float y = 0.0f;
    for (const std::string_view line : lines) {
        if (!line.empty()) {
            const Vec2 lineSize = renderer.measureText(line, scale);
            const float x = (maxWidth - lineSize.x) * 0.5f;
            layout.push_back({std::string(line), {x, y}});
        }
        y += lineHeight;
    }
    return layout;
}

void drawTextLayout(
    Renderer& renderer,
    Vec2 pos,
    const std::vector<KamishibaiTextLineLayout>& layout,
    Color color,
    int scale)
{
    for (const KamishibaiTextLineLayout& line : layout) {
        renderer.drawText(pos + line.offset, line.text, color, scale);
    }
}

void drawCenteredWrappedTextWithSoftShadow(
    Renderer& renderer,
    Vec2 pos,
    std::string_view text,
    float maxWidth,
    Color textColor,
    float alpha,
    int scale)
{
    const std::vector<KamishibaiTextLineLayout> layout = layoutCenteredWrappedText(renderer, text, maxWidth, scale);
    constexpr std::array<Vec2, 8> softOffsets{{
        {-2.0f, -1.0f},
        {0.0f, -2.0f},
        {2.0f, -1.0f},
        {-2.0f, 1.0f},
        {2.0f, 1.0f},
        {-1.0f, 2.0f},
        {1.0f, 2.0f},
        {0.0f, 3.0f},
    }};
    constexpr std::array<KamishibaiTextShadowLayer, 3> dropLayers{{
        {{3.0f, 3.0f}, {0, 0, 0, 42}},
        {{6.0f, 6.0f}, {0, 0, 0, 28}},
        {{9.0f, 9.0f}, {0, 0, 0, 16}},
    }};

    for (const Vec2 offset : softOffsets) {
        drawTextLayout(renderer, pos + offset, layout, multipliedAlpha({0, 0, 0, 32}, alpha), scale);
    }
    for (const KamishibaiTextShadowLayer& layer : dropLayers) {
        drawTextLayout(renderer, pos + layer.offset, layout, multipliedAlpha(layer.color, alpha), scale);
    }
    drawTextLayout(renderer, pos, layout, multipliedAlpha(textColor, alpha), scale);
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

bool parseNonNegativeFloat(std::string_view value, float& out)
{
    const std::string trimmed = trimAscii(value);
    if (trimmed.empty()) {
        return false;
    }
    char* end = nullptr;
    const float parsed = std::strtof(trimmed.c_str(), &end);
    if (end == trimmed.c_str() || (end != nullptr && *end != '\0') || !std::isfinite(parsed) || parsed < 0.0f) {
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

float transitionDurationForEffect(KamishibaiEffect effect)
{
    switch (effect) {
    case KamishibaiEffect::OverlayFade:
        return OverlayFadeSeconds;
    case KamishibaiEffect::None:
    case KamishibaiEffect::Flash:
    case KamishibaiEffect::FlashWhiteout:
    case KamishibaiEffect::ShakeDark:
    case KamishibaiEffect::TitleFade:
    case KamishibaiEffect::TitleFromBlack:
        return CrossFadeSeconds;
    }
    return CrossFadeSeconds;
}

bool usesPreviousPageAsUnderlay(KamishibaiEffect effect)
{
    return effect == KamishibaiEffect::OverlayFade;
}

bool fadesOutToWhite(KamishibaiEffect effect)
{
    return effect == KamishibaiEffect::FlashWhiteout;
}

float flashWhiteoutProgress(const KamishibaiPlayer& player, const KamishibaiPage& page)
{
    const float visibleSeconds = std::max(
        FlashWhiteoutFlashSeconds + 0.1f,
        page.duration - (player.currentIndex() > 0 ? player.transitionDuration() : 0.0f));
    const float riseSeconds = std::max(
        0.1f,
        visibleSeconds - FlashWhiteoutFlashSeconds - FlashWhiteoutHoldSeconds);
    return smoothStep((player.pageContentElapsed() - FlashWhiteoutFlashSeconds) / riseSeconds);
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
    page.textSteps = {page.text};
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
        trimAscii(columns[5]) == "note" &&
        (columns.size() < 7 || trimAscii(columns[6]) == "text_delay");
}

}

KamishibaiEffect kamishibaiEffectFromString(std::string_view value)
{
    const std::string normalized = lowerAscii(trimAscii(value));
    if (normalized == "flash") {
        return KamishibaiEffect::Flash;
    }
    if (normalized == "flash_whiteout") {
        return KamishibaiEffect::FlashWhiteout;
    }
    if (normalized == "shake_dark") {
        return KamishibaiEffect::ShakeDark;
    }
    if (normalized == "overlay_fade") {
        return KamishibaiEffect::OverlayFade;
    }
    if (normalized == "title_fade") {
        return KamishibaiEffect::TitleFade;
    }
    if (normalized == "title_from_black") {
        return KamishibaiEffect::TitleFromBlack;
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
    case KamishibaiEffect::FlashWhiteout:
        return "flash_whiteout";
    case KamishibaiEffect::ShakeDark:
        return "shake_dark";
    case KamishibaiEffect::OverlayFade:
        return "overlay_fade";
    case KamishibaiEffect::TitleFade:
        return "title_fade";
    case KamishibaiEffect::TitleFromBlack:
        return "title_from_black";
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
        result.warnings.push_back("opening kamishibai TSV header mismatch; expected id/image/text/duration/effect/note[/text_delay]");
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
        if (columns.size() > 7) {
            result.warnings.push_back(
                "opening kamishibai TSV line " + std::to_string(lineNumber) +
                " has extra columns; extras were ignored");
        }

        KamishibaiPage page;
        page.id = trimAscii(columns[0]);
        page.imagePath = trimAscii(columns[1]);
        page.textSteps = splitTextSteps(columns[2]);
        page.text = page.textSteps.empty() ? std::string{} : page.textSteps.front();
        page.effectName = lowerAscii(trimAscii(columns[4]));
        page.effect = kamishibaiEffectFromString(page.effectName);
        page.note = columns[5];
        if (columns.size() >= 7 && !trimAscii(columns[6]).empty() && !parseNonNegativeFloat(columns[6], page.textDelay)) {
            result.warnings.push_back(
                "opening kamishibai page " + pageLabel(page) +
                " has invalid text_delay; using 0");
        }

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
    underlayIndices_.assign(pages_.size(), -1);
    canSkipImmediately_ = canSkipImmediately;
    finished_ = pages_.empty();
    currentIndex_ = finished_ ? -1 : 0;
    previousIndex_ = -1;
    currentTextStepIndex_ = 0;
    pageElapsed_ = 0.0f;
    textStepElapsed_ = 0.0f;
    transitionElapsed_ = transitionDurationForEffect(KamishibaiEffect::None);
    transitionActive_ = false;
}

void KamishibaiPlayer::update(float dt)
{
    if (finished_ || pages_.empty()) {
        return;
    }

    const float safeDt = std::max(0.0f, dt);
    pageElapsed_ += safeDt;
    textStepElapsed_ += safeDt;
    if (transitionActive_) {
        transitionElapsed_ += safeDt;
        const float transitionSeconds = transitionDuration();
        if (transitionElapsed_ >= transitionSeconds) {
            transitionActive_ = false;
            previousIndex_ = -1;
            transitionElapsed_ = transitionSeconds;
        }
    }

    const KamishibaiPage* page = currentPage();
    const float duration = page != nullptr ? std::max(0.05f, page->duration) : DefaultPageDuration;
    if (textStepElapsed_ >= duration) {
        advance();
    }
}

void KamishibaiPlayer::finishImmediately()
{
    finished_ = true;
    previousIndex_ = -1;
    currentTextStepIndex_ = 0;
    transitionActive_ = false;
}

float KamishibaiPlayer::pageProgress() const
{
    const KamishibaiPage* page = currentPage();
    if (page == nullptr || page->duration <= 0.0f) {
        return 1.0f;
    }
    const int stepCount = std::max(1, static_cast<int>(page->textSteps.size()));
    return std::clamp(pageElapsed_ / (page->duration * static_cast<float>(stepCount)), 0.0f, 1.0f);
}

float KamishibaiPlayer::transitionProgress() const
{
    if (!transitionActive_) {
        return 1.0f;
    }
    return std::clamp(transitionElapsed_ / transitionDuration(), 0.0f, 1.0f);
}

float KamishibaiPlayer::transitionDuration() const
{
    const KamishibaiPage* page = currentPage();
    return transitionDurationForEffect(page != nullptr ? page->effect : KamishibaiEffect::None);
}

float KamishibaiPlayer::pageContentElapsed() const
{
    const float transitionDelay = currentIndex_ > 0 ? transitionDuration() : 0.0f;
    return std::max(0.0f, pageElapsed_ - transitionDelay);
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
    return pageAt(previousIndex_);
}

const KamishibaiPage* KamishibaiPlayer::pageAt(int index) const
{
    if (index < 0 || index >= static_cast<int>(pages_.size())) {
        return nullptr;
    }
    return &pages_[static_cast<std::size_t>(index)];
}

int KamishibaiPlayer::underlayIndex(int index) const
{
    if (index < 0 || index >= static_cast<int>(underlayIndices_.size())) {
        return -1;
    }
    return underlayIndices_[static_cast<std::size_t>(index)];
}

std::string_view KamishibaiPlayer::currentText() const
{
    const KamishibaiPage* page = currentPage();
    if (page == nullptr) {
        return {};
    }
    if (currentTextStepIndex_ >= 0 && currentTextStepIndex_ < static_cast<int>(page->textSteps.size())) {
        return page->textSteps[static_cast<std::size_t>(currentTextStepIndex_)];
    }
    return page->text;
}

bool KamishibaiPlayer::advance()
{
    if (finished_ || pages_.empty()) {
        return false;
    }

    const KamishibaiPage* page = currentPage();
    const int stepCount = page == nullptr ? 1 : std::max(1, static_cast<int>(page->textSteps.size()));
    if (currentTextStepIndex_ + 1 < stepCount) {
        ++currentTextStepIndex_;
        textStepElapsed_ = 0.0f;
        return true;
    }

    advancePage();
    return true;
}

void KamishibaiPlayer::advancePage()
{
    if (currentIndex_ + 1 >= static_cast<int>(pages_.size())) {
        finishImmediately();
        return;
    }

    previousIndex_ = currentIndex_;
    ++currentIndex_;
    if (const KamishibaiPage* page = currentPage(); page != nullptr && usesPreviousPageAsUnderlay(page->effect)) {
        underlayIndices_[static_cast<std::size_t>(currentIndex_)] = previousIndex_;
    }
    currentTextStepIndex_ = 0;
    pageElapsed_ = 0.0f;
    textStepElapsed_ = 0.0f;
    transitionElapsed_ = 0.0f;
    transitionActive_ = true;
}

void KamishibaiRenderer::render(
    Renderer& renderer,
    const KamishibaiPlayer& player,
    int screenWidth,
    int screenHeight,
    float shakeScale) const
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
        const float strength = 5.0f * (1.0f - smoothStep(progress)) * shakeScale;
        shakeOffset = {
            std::sin(player.pageElapsed() * 34.0f) * strength,
            std::sin(player.pageElapsed() * 47.0f + 1.1f) * strength * 0.7f,
        };
        overscan = 12.0f;
    } else if (current->effect == KamishibaiEffect::FlashWhiteout) {
        const float progress = flashWhiteoutProgress(player, *current);
        const float strength = 7.0f * progress * shakeScale;
        shakeOffset = {
            std::sin(player.pageElapsed() * 40.0f) * strength,
            std::sin(player.pageElapsed() * 53.0f + 0.8f) * strength * 0.72f,
        };
        overscan = 14.0f;
    }

    const float fade = smoothStep(player.transitionProgress());
    const bool overlayFade = current->effect == KamishibaiEffect::OverlayFade;
    if (current->effect == KamishibaiEffect::TitleFromBlack) {
        if (player.previousPage() != nullptr) {
            drawPageComposite(renderer, player, player.previousIndex(), width, height, 1.0f, {}, 0.0f);
            renderer.fillRect(
                {0.0f, 0.0f},
                {static_cast<float>(width), static_cast<float>(height)},
                {0, 0, 0, alphaByte(fade)});
        } else {
            const float titleAlpha = smoothStep(player.pageContentElapsed() / TitleFromBlackFadeInSeconds);
            drawCoverImage(renderer, *current, width, height, titleAlpha, {}, 0.0f);
        }
        return;
    }

    if (const KamishibaiPage* previous = player.previousPage()) {
        if (fadesOutToWhite(previous->effect)) {
            renderer.fillRect(
                {0.0f, 0.0f},
                {static_cast<float>(width), static_cast<float>(height)},
                {255, 255, 255, 255});
        } else {
            drawPageComposite(
                renderer,
                player,
                player.previousIndex(),
                width,
                height,
                overlayFade ? 1.0f : 1.0f - fade,
                {},
                0.0f);
        }
    }

    float currentAlpha = fade;
    if (current->effect == KamishibaiEffect::TitleFade) {
        currentAlpha *= smoothStep(player.pageElapsed() / 1.45f);
    }
    if (overlayFade && player.previousPage() != nullptr) {
        drawCoverImage(renderer, *current, width, height, currentAlpha, shakeOffset, overscan);
    } else {
        drawPageComposite(renderer, player, player.currentIndex(), width, height, currentAlpha, shakeOffset, overscan);
    }

    if (current->effect == KamishibaiEffect::ShakeDark) {
        const float darkAlpha = 0.38f * smoothStep((player.pageProgress() - 0.42f) / 0.58f);
        renderer.fillRect(
            {0.0f, 0.0f},
            {static_cast<float>(width), static_cast<float>(height)},
            {0, 0, 0, alphaByte(darkAlpha)});
    }
    if (current->effect == KamishibaiEffect::Flash) {
        const float flashElapsed = player.pageContentElapsed();
        if (player.currentIndex() == 0 || flashElapsed > 0.0f) {
            const float flash = std::max(0.0f, 1.0f - flashElapsed / 0.55f);
            const float alpha = flash * flash * 0.92f;
            renderer.fillRect(
                {0.0f, 0.0f},
                {static_cast<float>(width), static_cast<float>(height)},
                {255, 255, 255, alphaByte(alpha)});
        }
    }
    if (current->effect == KamishibaiEffect::FlashWhiteout) {
        const float elapsed = player.pageContentElapsed();
        const float whiteout = flashWhiteoutProgress(player, *current);
        if (whiteout > 0.0f) {
            renderer.fillRect(
                {0.0f, 0.0f},
                {static_cast<float>(width), static_cast<float>(height)},
                {255, 255, 255, alphaByte(whiteout)});
        } else if (player.currentIndex() == 0 || elapsed > 0.0f) {
            const float flash = std::max(0.0f, 1.0f - elapsed / FlashWhiteoutFlashSeconds);
            const float alpha = flash * flash * 0.92f;
            renderer.fillRect(
                {0.0f, 0.0f},
                {static_cast<float>(width), static_cast<float>(height)},
                {255, 255, 255, alphaByte(alpha)});
        }
    }
    if (current->effect == KamishibaiEffect::TitleFade) {
        const float black = 0.42f * (1.0f - smoothStep(player.pageElapsed() / 1.8f));
        renderer.fillRect(
            {0.0f, 0.0f},
            {static_cast<float>(width), static_cast<float>(height)},
            {0, 0, 0, alphaByte(black)});
    }

    const bool pageTransitionTextDelay = player.currentTextStepIndex() == 0 && player.currentIndex() > 0;
    const float textDelay = (pageTransitionTextDelay ? player.transitionDuration() : 0.0f) +
        TextAppearDelaySeconds +
        current->textDelay;
    const float textFadeIn = smoothStep((player.textStepElapsed() - textDelay) / TextFadeSeconds);
    const float textFadeOut = smoothStep((std::max(0.05f, current->duration) - player.textStepElapsed()) / TextFadeSeconds);
    const float textAlpha = std::min(textFadeIn, textFadeOut);
    drawTextBand(renderer, player.currentText(), width, height, textAlpha);
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

    const std::string title = "ダンジョンを掘る魔女";
    const int titleScale = width >= 1000 ? 7 : 5;
    const Vec2 titleSize = renderer.measureText(title, titleScale);
    const Vec2 titlePos{
        (static_cast<float>(width) - titleSize.x) * 0.5f,
        static_cast<float>(height) * 0.35f - titleSize.y * 0.5f,
    };
    renderer.drawOutlinedText(titlePos, title, {255, 248, 220, 255}, {0, 0, 0, 185}, 8, titleScale);
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
            RectF source{0.0f, 0.0f, imageSize.x, imageSize.y};
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

void KamishibaiRenderer::drawPageComposite(
    Renderer& renderer,
    const KamishibaiPlayer& player,
    int pageIndex,
    int screenWidth,
    int screenHeight,
    float alpha,
    Vec2 offset,
    float overscan) const
{
    const KamishibaiPage* page = player.pageAt(pageIndex);
    if (page == nullptr) {
        return;
    }
    const int underlay = player.underlayIndex(pageIndex);
    if (underlay >= 0 && underlay != pageIndex) {
        drawPageComposite(renderer, player, underlay, screenWidth, screenHeight, alpha, {}, 0.0f);
    }
    drawCoverImage(renderer, *page, screenWidth, screenHeight, alpha, offset, overscan);
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
    drawCenteredWrappedTextWithSoftShadow(
        renderer,
        pos,
        text,
        textWidth,
        {255, 255, 255, 245},
        alpha,
        scale);
}

}
