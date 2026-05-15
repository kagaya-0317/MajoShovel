#include "game/DialogueSystem.hpp"

#include "engine/Math.hpp"
#include "engine/Ui.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace majo {

namespace {

constexpr float DialogueCharacterDelay = 0.035f;
constexpr float DialogueCharacterFadeSeconds = 0.12f;
constexpr float DialoguePortraitFadeSeconds = 0.18f;
constexpr float DialoguePortraitSlidePixels = 24.0f;
constexpr float DialogueContentStartLeadFrames = 10.0f;
constexpr float DialoguePanelWidthRatio = 0.75f;
constexpr float DialoguePanelBottomMargin = 24.0f;
constexpr float DialoguePortraitWidth = 164.0f;
constexpr float DialogueTextPaddingX = 48.0f;
constexpr float DialogueTextPaddingTop = 44.0f;
constexpr float DialogueTextPaddingBottom = 18.0f;
constexpr int DialogueTextScale = 3;
constexpr std::string_view DialogueWindowId = "dialogue.message";
constexpr std::string_view DialogueHelpText = "F/Enter/Click/Esc 文字送り";

enum class DialoguePortraitSide {
    Left,
    Right,
};

struct DialogueGlyph {
    std::string text;
    Vec2 pos{};
    int revealIndex = 0;
};

struct DialoguePortraitSlot {
    std::string speakerId;
    std::string speakerName;
    std::string portraitPath;
    DialoguePortraitSide side = DialoguePortraitSide::Left;
};

std::vector<std::string> splitUtf8Codepoints(std::string_view text)
{
    std::vector<std::string> result;
    for (std::size_t i = 0; i < text.size();) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        std::size_t count = 1;
        if ((c & 0x80u) == 0x00u) {
            count = 1;
        } else if ((c & 0xE0u) == 0xC0u) {
            count = 2;
        } else if ((c & 0xF0u) == 0xE0u) {
            count = 3;
        } else if ((c & 0xF8u) == 0xF0u) {
            count = 4;
        }
        count = std::min(count, text.size() - i);
        result.emplace_back(text.substr(i, count));
        i += count;
    }
    return result;
}

bool isLineBreak(std::string_view codepoint)
{
    return codepoint == "\n" || codepoint == "\r";
}

int visibleGlyphCount(std::string_view text)
{
    int count = 0;
    for (const std::string& codepoint : splitUtf8Codepoints(text)) {
        if (!isLineBreak(codepoint)) {
            ++count;
        }
    }
    return count;
}

UiRect dialoguePanelRect(int screenWidth, int screenHeight)
{
    const float width = static_cast<float>(std::max(1, screenWidth));
    const float height = static_cast<float>(std::max(1, screenHeight));
    const float panelHeight = std::clamp(height * 0.30f, 190.0f, 252.0f);
    const float panelWidth = std::max(320.0f, width * DialoguePanelWidthRatio);
    return {
        {std::max(0.0f, (width - panelWidth) * 0.5f), std::max(0.0f, height - DialoguePanelBottomMargin - panelHeight)},
        {panelWidth, panelHeight},
    };
}

UiRect portraitRectFor(UiRect panel, DialoguePortraitSide side, int, int screenHeight)
{
    const float portraitHeight = std::clamp(static_cast<float>(std::max(1, screenHeight)) * 0.36f, 190.0f, 274.0f);
    const float x = side == DialoguePortraitSide::Right
        ? panel.pos.x + panel.size.x - DialoguePortraitWidth
        : panel.pos.x;
    return {{x, std::max(8.0f, panel.pos.y - portraitHeight + 18.0f)}, {DialoguePortraitWidth, portraitHeight}};
}

float easeOutSoft(float t)
{
    t = clamp(t, 0.0f, 1.0f);
    const float inv = 1.0f - t;
    return 1.0f - inv * inv;
}

float dialogueContentStartDelaySeconds()
{
    const float waitFrames = std::max(0.0f, ui::WindowAnimationFrames - DialogueContentStartLeadFrames);
    return ui::WindowAnimationSeconds * (waitFrames / std::max(1.0f, ui::WindowAnimationFrames));
}

Vec2 portraitSlideOffset(DialoguePortraitSide side, float fade)
{
    const float hiddenOffset = side == DialoguePortraitSide::Right
        ? DialoguePortraitSlidePixels
        : -DialoguePortraitSlidePixels;
    return {hiddenOffset * (1.0f - easeOutSoft(fade)), 0.0f};
}

UiRect textRectFor(UiRect panel)
{
    const float footerHeight = uiFooterHeight(DialogueHelpText);
    const float x = panel.pos.x + DialogueTextPaddingX;
    return {
        {x, panel.pos.y + DialogueTextPaddingTop},
        {
            std::max(1.0f, panel.size.x - DialogueTextPaddingX * 2.0f),
            std::max(1.0f, panel.size.y - DialogueTextPaddingTop - DialogueTextPaddingBottom - footerHeight),
        },
    };
}

float dialogueNativeMeasurePadding(Renderer& renderer)
{
    const float single = renderer.measureText("あ", DialogueTextScale).x;
    const float doubled = renderer.measureText("ああ", DialogueTextScale).x;
    return std::max(0.0f, single * 2.0f - doubled);
}

float dialogueLineWidth(Renderer& renderer, std::string_view text)
{
    return std::max(0.0f, renderer.measureText(text, DialogueTextScale).x - dialogueNativeMeasurePadding(renderer));
}

void appendDialogueGlyphLine(
    Renderer& renderer,
    const std::vector<std::string>& line,
    UiRect textRect,
    float y,
    int& revealIndex,
    std::vector<DialogueGlyph>& glyphs)
{
    std::string prefix;
    for (const std::string& codepoint : line) {
        const float x = prefix.empty() ? 0.0f : dialogueLineWidth(renderer, prefix);
        glyphs.push_back(DialogueGlyph{codepoint, textRect.pos + Vec2{x, y}, revealIndex});
        prefix += codepoint;
        ++revealIndex;
    }
}

std::vector<DialogueGlyph> layoutDialogueGlyphs(Renderer& renderer, std::string_view text, UiRect textRect)
{
    std::vector<DialogueGlyph> glyphs;
    const Vec2 lineMeasure = renderer.measureText("あ", DialogueTextScale);
    const float lineHeight = std::max(24.0f, lineMeasure.y + 6.0f);

    std::vector<std::string> line;
    std::string lineText;
    float y = 0.0f;
    int revealIndex = 0;
    for (const std::string& codepoint : splitUtf8Codepoints(text)) {
        if (codepoint == "\r") {
            continue;
        }
        if (codepoint == "\n") {
            appendDialogueGlyphLine(renderer, line, textRect, y, revealIndex, glyphs);
            line.clear();
            lineText.clear();
            y += lineHeight;
            continue;
        }

        const std::string candidate = lineText + codepoint;
        if (!line.empty() && dialogueLineWidth(renderer, candidate) > textRect.size.x) {
            appendDialogueGlyphLine(renderer, line, textRect, y, revealIndex, glyphs);
            line.clear();
            lineText.clear();
            y += lineHeight;
        }

        line.push_back(codepoint);
        lineText += codepoint;
    }

    appendDialogueGlyphLine(renderer, line, textRect, y, revealIndex, glyphs);
    return glyphs;
}

Color withAlpha(Color color, unsigned char alpha)
{
    color.a = alpha;
    return color;
}

Color fadeColor(Color color, float alphaScale)
{
    color.a = static_cast<unsigned char>(std::clamp(std::round(static_cast<float>(color.a) * alphaScale), 0.0f, 255.0f));
    return color;
}

unsigned char fadedAlpha(float lineElapsed, int revealIndex)
{
    const float age = lineElapsed - static_cast<float>(revealIndex) * DialogueCharacterDelay;
    if (age <= 0.0f) {
        return 0;
    }
    const float t = std::clamp(age / DialogueCharacterFadeSeconds, 0.0f, 1.0f);
    const int quantized = static_cast<int>(std::round(t * 15.0f)) * 17;
    return static_cast<unsigned char>(std::clamp(quantized, 0, 255));
}

Color portraitColorFor(std::string_view speakerId)
{
    if (speakerId == "monica") {
        return {88, 128, 214, 255};
    }
    if (speakerId == "chicory") {
        return {230, 212, 112, 255};
    }
    if (speakerId == "player") {
        return {132, 86, 178, 255};
    }
    return {156, 168, 184, 255};
}

std::string speakerNameForId(std::string_view speakerId)
{
    if (speakerId == "monica") {
        return "モニカ";
    }
    if (speakerId == "player") {
        return "主人公";
    }
    if (speakerId == "chicory") {
        return "チコリ";
    }
    return std::string(speakerId);
}

std::string portraitPathForId(std::string_view)
{
    return {};
}

DialoguePortraitSlot portraitSlotFor(const DialogueSequence& sequence, std::string_view speakerId, DialoguePortraitSide side)
{
    DialoguePortraitSlot slot{
        std::string(speakerId),
        speakerNameForId(speakerId),
        portraitPathForId(speakerId),
        side,
    };

    for (const DialogueLine& line : sequence.lines) {
        if (line.speakerId != speakerId) {
            continue;
        }
        if (!line.speakerName.empty()) {
            slot.speakerName = line.speakerName;
        }
        if (!line.portraitPath.empty()) {
            slot.portraitPath = line.portraitPath;
        }
        break;
    }
    return slot;
}

std::string rightPortraitSpeakerId(const DialogueSequence& sequence, const DialogueLine* currentLine)
{
    if (currentLine != nullptr && currentLine->speakerId != "player") {
        return currentLine->speakerId;
    }
    for (const DialogueLine& line : sequence.lines) {
        if (!line.speakerId.empty() && line.speakerId != "player") {
            return line.speakerId;
        }
    }
    return {};
}

std::vector<DialoguePortraitSlot> portraitSlotsFor(const DialogueSequence& sequence, const DialogueLine* currentLine)
{
    std::vector<DialoguePortraitSlot> slots;
    slots.push_back(portraitSlotFor(sequence, "player", DialoguePortraitSide::Left));

    const std::string rightSpeaker = rightPortraitSpeakerId(sequence, currentLine);
    if (!rightSpeaker.empty()) {
        slots.push_back(portraitSlotFor(sequence, rightSpeaker, DialoguePortraitSide::Right));
    }
    return slots;
}

void drawPortraitPlaceholder(Renderer& renderer, UiRect rect, const DialoguePortraitSlot& slot, float alpha)
{
    const Color base = portraitColorFor(slot.speakerId);
    const Vec2 center = rect.pos + rect.size * 0.5f;
    renderer.fillEllipse(center + Vec2{0.0f, 58.0f}, {50.0f, 82.0f}, fadeColor(withAlpha(base, 218), alpha));
    renderer.fillCircle(center + Vec2{0.0f, -12.0f}, 36.0f, fadeColor({240, 218, 188, 255}, alpha));
    renderer.fillCircle(center + Vec2{-12.0f, -16.0f}, 4.0f, fadeColor({42, 34, 44, 255}, alpha));
    renderer.fillCircle(center + Vec2{12.0f, -16.0f}, 4.0f, fadeColor({42, 34, 44, 255}, alpha));
    renderer.drawLine(center + Vec2{-8.0f, 2.0f}, center + Vec2{8.0f, 2.0f}, fadeColor({116, 72, 84, 255}, alpha));

    const Vec2 hatPoints[3] = {
        center + Vec2{-54.0f, -36.0f},
        center + Vec2{54.0f, -36.0f},
        center + Vec2{4.0f, -104.0f},
    };
    renderer.fillPolygon(hatPoints, 3, fadeColor(withAlpha(base, 230), alpha));
    renderer.drawLine(hatPoints[0], hatPoints[1], fadeColor({236, 242, 255, 210}, alpha));
    renderer.drawLine(hatPoints[1], hatPoints[2], fadeColor({236, 242, 255, 150}, alpha));
    renderer.drawLine(hatPoints[2], hatPoints[0], fadeColor({236, 242, 255, 150}, alpha));
}

void drawPortrait(Renderer& renderer, UiRect rect, const DialoguePortraitSlot& slot, float alpha)
{
    if (!slot.portraitPath.empty()) {
        ImageDrawOptions options;
        options.anchor = {0.5f, 1.0f};
        options.tint = fadeColor(options.tint, alpha);
        const bool drew = renderer.drawImage(
            slot.portraitPath,
            {rect.pos.x + rect.size.x * 0.5f, rect.pos.y + rect.size.y - 6.0f},
            {rect.size.x, rect.size.y},
            options,
            TextureFilter::Linear);
        if (drew) {
            return;
        }
    }
    drawPortraitPlaceholder(renderer, rect, slot, alpha);
}

void drawSpeakerName(Renderer& renderer, UiRect panel, const DialogueLine& line, float alpha)
{
    if (line.speakerName.empty()) {
        return;
    }
    constexpr int NameScale = 3;
    const Vec2 nameSize = renderer.measureText(line.speakerName, NameScale);
    const Vec2 namePos{
        panel.pos.x + DialogueTextPaddingX - 48.0f,
        panel.pos.y - nameSize.y + 14.0f,
    };
    renderer.drawOutlinedText(
        namePos,
        line.speakerName,
        fadeColor({255, 244, 210, 255}, alpha),
        fadeColor({0, 0, 0, 170}, alpha),
        6,
        NameScale);
}

bool advancePressed(const Input& input)
{
    return input.confirmPressed() ||
        input.useItemPressed() ||
        input.mouseLeftPressed() ||
        input.backPressed();
}

}

void DialoguePlayer::start(DialogueSequence sequence)
{
    sequence_ = std::move(sequence);
    lineIndex_ = 0;
    openElapsed_ = 0.0f;
    lineElapsed_ = 0.0f;
    contentFade_ = 0.0f;
    active_ = !sequence_.lines.empty();
    closing_ = false;
}

void DialoguePlayer::clear()
{
    sequence_ = DialogueSequence{};
    lineIndex_ = 0;
    openElapsed_ = 0.0f;
    lineElapsed_ = 0.0f;
    contentFade_ = 0.0f;
    active_ = false;
    closing_ = false;
}

void DialoguePlayer::update(const Input& input, float dt)
{
    if (!active_) {
        return;
    }

    const float contentStartDelay = dialogueContentStartDelaySeconds();
    if (openElapsed_ < contentStartDelay) {
        openElapsed_ = std::min(contentStartDelay, openElapsed_ + std::max(0.0f, dt));
        return;
    }

    const float fadeStep = dt / std::max(0.001f, DialoguePortraitFadeSeconds);
    if (closing_) {
        contentFade_ = std::max(0.0f, contentFade_ - fadeStep);
        if (contentFade_ <= 0.0f) {
            clear();
        }
        return;
    }

    contentFade_ = std::min(1.0f, contentFade_ + fadeStep);
    lineElapsed_ += std::max(0.0f, dt);
    if (advancePressed(input)) {
        if (lineComplete()) {
            advanceLine();
        } else {
            revealCurrentLine();
        }
    }
}

void DialoguePlayer::render(Renderer& renderer, int screenWidth, int screenHeight) const
{
    if (!active_) {
        return;
    }
    const DialogueLine* line = currentLine();
    if (line == nullptr) {
        return;
    }

    renderer.setScreenSpace();
    const UiRect panel = dialoguePanelRect(screenWidth, screenHeight);
    const UiRect textRect = textRectFor(panel);

    const float contentStartDelay = dialogueContentStartDelaySeconds();
    const bool contentVisible = closing_ || openElapsed_ >= contentStartDelay;
    if (!contentVisible) {
        UiWindowScope dialogueWindow(
            renderer,
            DialogueWindowId,
            panel,
            "",
            DialogueHelpText,
            UiWindowOptions{true, false});
        return;
    }

    for (const DialoguePortraitSlot& slot : portraitSlotsFor(sequence_, line)) {
        UiRect portrait = portraitRectFor(panel, slot.side, screenWidth, screenHeight);
        portrait.pos += portraitSlideOffset(slot.side, contentFade_);
        drawPortrait(renderer, portrait, slot, contentFade_);
    }

    if (closing_) {
        return;
    }

    UiWindowScope dialogueWindow(
        renderer,
        DialogueWindowId,
        panel,
        "",
        DialogueHelpText,
        UiWindowOptions{true, false});
    drawSpeakerName(renderer, panel, *line, contentFade_);

    const std::vector<DialogueGlyph> glyphs = layoutDialogueGlyphs(renderer, line->text, textRect);
    for (const DialogueGlyph& glyph : glyphs) {
        const unsigned char alpha = fadedAlpha(lineElapsed_, glyph.revealIndex);
        if (alpha == 0) {
            continue;
        }
        renderer.drawText(glyph.pos + Vec2{2.0f, 2.0f}, glyph.text, {0, 0, 0, static_cast<unsigned char>(alpha * 3 / 4)}, DialogueTextScale);
        renderer.drawText(glyph.pos, glyph.text, {255, 255, 255, alpha}, DialogueTextScale);
    }

}

bool DialoguePlayer::lineComplete() const
{
    if (!active_ || closing_) {
        return true;
    }
    return lineElapsed_ >= currentLineCompletionTime();
}

const DialogueLine* DialoguePlayer::currentLine() const
{
    if (!active_ || lineIndex_ < 0 || lineIndex_ >= static_cast<int>(sequence_.lines.size())) {
        if (closing_ && !sequence_.lines.empty()) {
            return &sequence_.lines.back();
        }
        return nullptr;
    }
    return &sequence_.lines[static_cast<std::size_t>(lineIndex_)];
}

int DialoguePlayer::currentLineGlyphCount() const
{
    const DialogueLine* line = currentLine();
    return line != nullptr ? visibleGlyphCount(line->text) : 0;
}

float DialoguePlayer::currentLineCompletionTime() const
{
    const int count = currentLineGlyphCount();
    if (count <= 0) {
        return 0.0f;
    }
    return static_cast<float>(count - 1) * DialogueCharacterDelay + DialogueCharacterFadeSeconds;
}

void DialoguePlayer::revealCurrentLine()
{
    lineElapsed_ = currentLineCompletionTime();
}

void DialoguePlayer::advanceLine()
{
    if (!active_) {
        return;
    }

    if (lineIndex_ + 1 >= static_cast<int>(sequence_.lines.size())) {
        closing_ = true;
        revealCurrentLine();
        return;
    }

    ++lineIndex_;
    lineElapsed_ = 0.0f;
}

}
