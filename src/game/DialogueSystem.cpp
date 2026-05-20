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
constexpr float DialogueAdvanceRepeatInterval = 0.28f;
constexpr float DialogueContentStartLeadFrames = 10.0f;
constexpr float DialoguePanelWidthRatio = 0.75f;
constexpr float DialoguePanelBottomMargin = 24.0f;
constexpr float DialoguePortraitWidth = 164.0f;
constexpr float DialoguePortraitInsetX = 48.0f;
constexpr float DialogueTextPaddingX = 48.0f;
constexpr float DialogueTextPaddingTop = 44.0f;
constexpr float DialogueTextPaddingBottom = 18.0f;
constexpr float DialogueInactivePortraitBrightness = 0.68f;
constexpr int DialogueTextScale = 3;
constexpr std::string_view DialogueWindowId = "dialogue.message";
constexpr std::string_view DialogueHelpText = "F/Enter/Click/Esc 文字送り";
constexpr std::string_view MonicaCallHelpText = "F/Enter/Click/Esc 文字送り";

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
        ? panel.pos.x + panel.size.x - DialoguePortraitWidth - DialoguePortraitInsetX
        : panel.pos.x + DialoguePortraitInsetX;
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

UiRect monicaCallPhoneRect(int screenWidth, int screenHeight)
{
    const float width = static_cast<float>(std::max(1, screenWidth));
    const float height = static_cast<float>(std::max(1, screenHeight));
    const float phoneWidth = std::clamp(width * 0.11f, 108.0f, 148.0f);
    const float phoneHeight = phoneWidth * 1.72f;
    return {
        {std::clamp(width * 0.08f, 28.0f, 126.0f), std::max(22.0f, height - phoneHeight - 36.0f)},
        {phoneWidth, phoneHeight},
    };
}

UiRect monicaCallBubbleRect(int screenWidth, int screenHeight, UiRect phone)
{
    const float width = static_cast<float>(std::max(1, screenWidth));
    const float height = static_cast<float>(std::max(1, screenHeight));
    const float bubbleWidth = std::clamp(width * 0.70f, 560.0f, 840.0f);
    const float bubbleHeight = std::clamp(height * 0.25f, 178.0f, 220.0f);
    const float bubbleX = std::clamp(
        phone.pos.x + phone.size.x * 0.82f,
        22.0f,
        std::max(22.0f, width - bubbleWidth - 24.0f));
    return {
        {bubbleX, std::max(18.0f, height - bubbleHeight - 46.0f)},
        {bubbleWidth, bubbleHeight},
    };
}

void fillRoundedRectLocal(Renderer& renderer, UiRect rect, float radius, Color color)
{
    if (rect.size.x <= 0.0f || rect.size.y <= 0.0f || color.a == 0) {
        return;
    }
    const float r = std::min({std::max(0.0f, radius), rect.size.x * 0.5f, rect.size.y * 0.5f});
    if (r <= 0.0f) {
        renderer.fillRect(rect.pos, rect.size, color);
        return;
    }

    renderer.fillRect({rect.pos.x + r, rect.pos.y}, {std::max(0.0f, rect.size.x - r * 2.0f), rect.size.y}, color);
    renderer.fillRect({rect.pos.x, rect.pos.y + r}, {r, std::max(0.0f, rect.size.y - r * 2.0f)}, color);
    renderer.fillRect({rect.pos.x + rect.size.x - r, rect.pos.y + r}, {r, std::max(0.0f, rect.size.y - r * 2.0f)}, color);
    renderer.fillCircle(rect.pos + Vec2{r, r}, r, color);
    renderer.fillCircle(rect.pos + Vec2{rect.size.x - r, r}, r, color);
    renderer.fillCircle(rect.pos + Vec2{r, rect.size.y - r}, r, color);
    renderer.fillCircle(rect.pos + rect.size - Vec2{r, r}, r, color);
}

bool sequenceUsesMonicaCallUi(const DialogueSequence& sequence)
{
    return sequence.id.rfind("monica_radio_", 0) == 0;
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

Color portraitToneColor(Color color, float alphaScale, float brightness)
{
    brightness = std::clamp(brightness, 0.0f, 1.0f);
    color.r = static_cast<unsigned char>(std::clamp(std::round(static_cast<float>(color.r) * brightness), 0.0f, 255.0f));
    color.g = static_cast<unsigned char>(std::clamp(std::round(static_cast<float>(color.g) * brightness), 0.0f, 255.0f));
    color.b = static_cast<unsigned char>(std::clamp(std::round(static_cast<float>(color.b) * brightness), 0.0f, 255.0f));
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
    if (speakerId == "elder") {
        return {126, 154, 116, 255};
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
        return "ルネ";
    }
    if (speakerId == "chicory") {
        return "チコリ";
    }
    if (speakerId == "elder") {
        return "村長";
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

void drawPortraitPlaceholder(Renderer& renderer, UiRect rect, const DialoguePortraitSlot& slot, float alpha, float brightness)
{
    const Color base = portraitColorFor(slot.speakerId);
    const Vec2 center = rect.pos + rect.size * 0.5f;
    renderer.fillEllipse(center + Vec2{0.0f, 58.0f}, {50.0f, 82.0f}, portraitToneColor(withAlpha(base, 218), alpha, brightness));
    renderer.fillCircle(center + Vec2{0.0f, -12.0f}, 36.0f, portraitToneColor({240, 218, 188, 255}, alpha, brightness));
    renderer.fillCircle(center + Vec2{-12.0f, -16.0f}, 4.0f, portraitToneColor({42, 34, 44, 255}, alpha, brightness));
    renderer.fillCircle(center + Vec2{12.0f, -16.0f}, 4.0f, portraitToneColor({42, 34, 44, 255}, alpha, brightness));
    renderer.drawLine(center + Vec2{-8.0f, 2.0f}, center + Vec2{8.0f, 2.0f}, portraitToneColor({116, 72, 84, 255}, alpha, brightness));

    const Vec2 hatPoints[3] = {
        center + Vec2{-54.0f, -36.0f},
        center + Vec2{54.0f, -36.0f},
        center + Vec2{4.0f, -104.0f},
    };
    renderer.fillPolygon(hatPoints, 3, portraitToneColor(withAlpha(base, 230), alpha, brightness));
    renderer.drawLine(hatPoints[0], hatPoints[1], portraitToneColor({236, 242, 255, 210}, alpha, brightness));
    renderer.drawLine(hatPoints[1], hatPoints[2], portraitToneColor({236, 242, 255, 150}, alpha, brightness));
    renderer.drawLine(hatPoints[2], hatPoints[0], portraitToneColor({236, 242, 255, 150}, alpha, brightness));
}

void drawPortrait(Renderer& renderer, UiRect rect, const DialoguePortraitSlot& slot, float alpha, float brightness = 1.0f)
{
    if (!slot.portraitPath.empty()) {
        ImageDrawOptions options;
        options.anchor = {0.5f, 1.0f};
        options.tint = portraitToneColor(options.tint, alpha, brightness);
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
    drawPortraitPlaceholder(renderer, rect, slot, alpha, brightness);
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

bool advanceHeld(const Input& input)
{
    return input.held(InputAction::Confirm) ||
        input.held(InputAction::UseSelectedItem) ||
        input.mouseLeftHeld() ||
        input.backHeld();
}

}

void DialoguePlayer::start(DialogueSequence sequence)
{
    if (sequence.steps.empty()) {
        sequence.steps.reserve(sequence.lines.size());
        for (const DialogueLine& line : sequence.lines) {
            sequence.steps.push_back(DialogueStep{DialogueStepKind::Line, line, 0.0f});
        }
    }
    if (sequence.lines.empty()) {
        for (const DialogueStep& step : sequence.steps) {
            if (step.kind == DialogueStepKind::Line) {
                sequence.lines.push_back(step.line);
            }
        }
    }

    sequence_ = std::move(sequence);
    stepIndex_ = 0;
    openElapsed_ = 0.0f;
    lineElapsed_ = 0.0f;
    contentFade_ = 0.0f;
    resetAdvanceHoldRepeat();
    rightSpeakerId_.clear();
    pendingRightSpeakerId_.clear();
    rightPortraitFade_ = 0.0f;
    rightPortraitTransition_ = RightPortraitTransition::Stable;
    active_ = !sequence_.steps.empty();
    closing_ = false;
    syncRightPortraitForCurrentLine(false);
}

void DialoguePlayer::clear()
{
    sequence_ = DialogueSequence{};
    stepIndex_ = 0;
    openElapsed_ = 0.0f;
    lineElapsed_ = 0.0f;
    contentFade_ = 0.0f;
    resetAdvanceHoldRepeat();
    rightSpeakerId_.clear();
    pendingRightSpeakerId_.clear();
    rightPortraitFade_ = 0.0f;
    rightPortraitTransition_ = RightPortraitTransition::Stable;
    active_ = false;
    closing_ = false;
}

void DialoguePlayer::update(const Input& input, float dt)
{
    if (!active_) {
        return;
    }

    const float safeDt = std::max(0.0f, dt);
    const float contentStartDelay = dialogueContentStartDelaySeconds();
    if (openElapsed_ < contentStartDelay) {
        resetAdvanceHoldRepeat();
        openElapsed_ = std::min(contentStartDelay, openElapsed_ + safeDt);
        return;
    }

    const float fadeStep = safeDt / std::max(0.001f, DialoguePortraitFadeSeconds);
    if (closing_) {
        resetAdvanceHoldRepeat();
        contentFade_ = std::max(0.0f, contentFade_ - fadeStep);
        if (contentFade_ <= 0.0f) {
            clear();
        }
        return;
    }

    contentFade_ = std::min(1.0f, contentFade_ + fadeStep);
    updateRightPortrait(safeDt);
    lineElapsed_ += safeDt;

    const DialogueStep* step = currentStep();
    if (step != nullptr && step->kind == DialogueStepKind::Wait) {
        resetAdvanceHoldRepeat();
        if (lineComplete()) {
            advanceLine();
        }
        return;
    }

    if (advanceRequested(input, safeDt)) {
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

    if (sequenceUsesMonicaCallUi(sequence_)) {
        renderMonicaCall(renderer, screenWidth, screenHeight, line);
        return;
    }

    renderer.setScreenSpace();
    const UiRect panel = dialoguePanelRect(screenWidth, screenHeight);
    const UiRect textRect = textRectFor(panel);

    const float contentStartDelay = dialogueContentStartDelaySeconds();
    const bool contentVisible = closing_ || openElapsed_ >= contentStartDelay;
    if (!contentVisible) {
        return;
    }

    const std::string_view currentSpeaker = line != nullptr ? std::string_view(line->speakerId) : std::string_view{};
    const auto portraitBrightness = [currentSpeaker](std::string_view speakerId) {
        if (currentSpeaker.empty() || speakerId == currentSpeaker) {
            return 1.0f;
        }
        return DialogueInactivePortraitBrightness;
    };

    if (spokenLineSeen()) {
        const DialoguePortraitSlot leftSlot = portraitSlotFor(sequence_, "player", DialoguePortraitSide::Left);
        UiRect leftPortrait = portraitRectFor(panel, DialoguePortraitSide::Left, screenWidth, screenHeight);
        leftPortrait.pos += portraitSlideOffset(DialoguePortraitSide::Left, contentFade_);
        drawPortrait(renderer, leftPortrait, leftSlot, contentFade_, portraitBrightness("player"));
    }

    if (!rightSpeakerId_.empty() && rightPortraitFade_ > 0.0f) {
        const float rightAlpha = std::min(contentFade_, rightPortraitFade_);
        if (rightAlpha > 0.0f) {
            const DialoguePortraitSlot rightSlot = portraitSlotFor(sequence_, rightSpeakerId_, DialoguePortraitSide::Right);
            UiRect rightPortrait = portraitRectFor(panel, DialoguePortraitSide::Right, screenWidth, screenHeight);
            if (rightPortraitTransition_ == RightPortraitTransition::FadingIn || closing_) {
                rightPortrait.pos += portraitSlideOffset(DialoguePortraitSide::Right, rightAlpha);
            }
            drawPortrait(renderer, rightPortrait, rightSlot, rightAlpha, portraitBrightness(rightSpeakerId_));
        }
    }

    if (closing_) {
        return;
    }
    if (line == nullptr) {
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

void DialoguePlayer::renderMonicaCall(Renderer& renderer, int screenWidth, int screenHeight, const DialogueLine* line) const
{
    renderer.setScreenSpace();

    const float contentStartDelay = dialogueContentStartDelaySeconds();
    const bool contentVisible = closing_ || openElapsed_ >= contentStartDelay;
    if (!contentVisible) {
        return;
    }

    const UiRect phone = monicaCallPhoneRect(screenWidth, screenHeight);
    const UiRect bubble = monicaCallBubbleRect(screenWidth, screenHeight, phone);
    const float alpha = contentFade_;
    const auto a = [alpha](Color color) {
        return fadeColor(color, alpha);
    };

    renderer.fillRect(
        {0.0f, 0.0f},
        {static_cast<float>(std::max(1, screenWidth)), static_cast<float>(std::max(1, screenHeight))},
        a({0, 0, 0, 42}));

    fillRoundedRectLocal(renderer, {phone.pos + Vec2{8.0f, 10.0f}, phone.size}, 20.0f, a({0, 0, 0, 96}));
    fillRoundedRectLocal(renderer, phone, 20.0f, a({18, 22, 34, 245}));
    const UiRect screen{{phone.pos.x + 10.0f, phone.pos.y + 20.0f}, {phone.size.x - 20.0f, phone.size.y - 44.0f}};
    fillRoundedRectLocal(renderer, screen, 12.0f, a({46, 72, 116, 244}));
    renderer.fillGradientRect(screen.pos, screen.size, a({38, 78, 136, 230}), a({112, 176, 210, 210}), GradientDirection::TopToBottom);
    renderer.fillCircle({phone.pos.x + phone.size.x * 0.5f, phone.pos.y + 10.0f}, 3.0f, a({104, 118, 144, 230}));
    renderer.drawCircle({phone.pos.x + phone.size.x * 0.5f, phone.pos.y + phone.size.y - 14.0f}, 7.0f, a({110, 126, 154, 210}));

    const DialoguePortraitSlot monicaSlot = portraitSlotFor(sequence_, "monica", DialoguePortraitSide::Right);
    drawPortrait(
        renderer,
        {{screen.pos.x + screen.size.x * 0.18f, screen.pos.y + 16.0f}, {screen.size.x * 0.64f, screen.size.y - 20.0f}},
        monicaSlot,
        alpha,
        1.0f);

    const Vec2 tail[3] = {
        {bubble.pos.x + 38.0f, bubble.pos.y + bubble.size.y - 48.0f},
        {phone.pos.x + phone.size.x * 0.76f, phone.pos.y + phone.size.y * 0.30f},
        {bubble.pos.x + 38.0f, bubble.pos.y + bubble.size.y - 94.0f},
    };
    renderer.fillPolygon(tail, 3, a({238, 248, 255, 236}));
    fillRoundedRectLocal(renderer, {bubble.pos + Vec2{8.0f, 10.0f}, bubble.size}, 24.0f, a({0, 0, 0, 86}));
    fillRoundedRectLocal(renderer, bubble, 24.0f, a({238, 248, 255, 238}));
    renderer.drawRect(bubble.pos, bubble.size, a({120, 182, 220, 230}));

    const UiRect portraitFrame{{bubble.pos.x + 24.0f, bubble.pos.y + 28.0f}, {132.0f, bubble.size.y - 56.0f}};
    fillRoundedRectLocal(renderer, portraitFrame, 16.0f, a({48, 84, 136, 226}));
    renderer.fillGradientRect(
        portraitFrame.pos,
        portraitFrame.size,
        a({38, 62, 112, 210}),
        a({118, 178, 214, 190}),
        GradientDirection::TopToBottom);
    drawPortrait(renderer, portraitFrame, monicaSlot, alpha, 1.0f);

    renderer.fillCircle({bubble.pos.x + bubble.size.x - 34.0f, bubble.pos.y + 32.0f}, 5.0f, a({72, 218, 164, 255}));
    renderer.drawText({bubble.pos.x + 176.0f, bubble.pos.y + 24.0f}, "モニカ通信", a({38, 76, 122, 255}), 2);
    renderer.drawText({bubble.pos.x + 176.0f, bubble.pos.y + 48.0f}, line != nullptr && !line->speakerName.empty() ? line->speakerName : "モニカ", a({24, 32, 54, 255}), 3);

    if (closing_ || line == nullptr) {
        return;
    }

    const float footerHeight = uiFooterHeight(MonicaCallHelpText);
    const UiRect textRect{
        {bubble.pos.x + 176.0f, bubble.pos.y + 84.0f},
        {
            std::max(1.0f, bubble.size.x - 212.0f),
            std::max(1.0f, bubble.size.y - 98.0f - footerHeight),
        },
    };
    const std::vector<DialogueGlyph> glyphs = layoutDialogueGlyphs(renderer, line->text, textRect);
    for (const DialogueGlyph& glyph : glyphs) {
        const unsigned char glyphAlpha = fadedAlpha(lineElapsed_, glyph.revealIndex);
        if (glyphAlpha == 0) {
            continue;
        }
        const unsigned char scaled = static_cast<unsigned char>(std::clamp(
            std::lround(static_cast<float>(glyphAlpha) * alpha),
            0L,
            255L));
        renderer.drawText(glyph.pos + Vec2{1.0f, 1.0f}, glyph.text, {255, 255, 255, static_cast<unsigned char>(scaled / 2)}, DialogueTextScale);
        renderer.drawText(glyph.pos, glyph.text, {24, 30, 46, scaled}, DialogueTextScale);
    }

    renderer.drawText(
        {bubble.pos.x + 176.0f, bubble.pos.y + bubble.size.y - footerHeight + 8.0f},
        MonicaCallHelpText,
        a({86, 104, 128, 255}),
        2);
}

bool DialoguePlayer::lineComplete() const
{
    if (!active_ || closing_) {
        return true;
    }
    const DialogueStep* step = currentStep();
    if (step != nullptr && step->kind == DialogueStepKind::Wait) {
        return lineElapsed_ >= std::max(0.0f, step->waitSeconds);
    }
    return lineElapsed_ >= currentLineCompletionTime();
}

const DialogueStep* DialoguePlayer::currentStep() const
{
    if (!active_ || stepIndex_ < 0 || stepIndex_ >= static_cast<int>(sequence_.steps.size())) {
        return nullptr;
    }
    return &sequence_.steps[static_cast<std::size_t>(stepIndex_)];
}

const DialogueLine* DialoguePlayer::currentLine() const
{
    const DialogueStep* step = currentStep();
    if (step == nullptr || step->kind != DialogueStepKind::Line) {
        return nullptr;
    }
    return &step->line;
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

bool DialoguePlayer::spokenLineSeen() const
{
    if (!active_) {
        return false;
    }
    const int last = std::min(stepIndex_, static_cast<int>(sequence_.steps.size()) - 1);
    for (int i = 0; i <= last; ++i) {
        const DialogueStep& step = sequence_.steps[static_cast<std::size_t>(i)];
        if (step.kind == DialogueStepKind::Line && !step.line.speakerId.empty()) {
            return true;
        }
    }
    return false;
}

bool DialoguePlayer::advanceRequested(const Input& input, float dt)
{
    const bool pressed = advancePressed(input);
    if (!advanceHeld(input)) {
        resetAdvanceHoldRepeat();
        return pressed;
    }

    if (!advanceHoldActive_) {
        advanceHoldActive_ = true;
        advanceRepeatTimer_ = DialogueAdvanceRepeatInterval;
        return pressed;
    }

    const float safeDt = std::max(0.0f, dt);
    advanceRepeatTimer_ -= safeDt;
    if (advanceRepeatTimer_ > 0.0f) {
        return pressed;
    }

    do {
        advanceRepeatTimer_ += DialogueAdvanceRepeatInterval;
    } while (advanceRepeatTimer_ <= 0.0f);
    return true;
}

void DialoguePlayer::revealCurrentLine()
{
    lineElapsed_ = currentLineCompletionTime();
}

void DialoguePlayer::resetAdvanceHoldRepeat()
{
    advanceHoldActive_ = false;
    advanceRepeatTimer_ = 0.0f;
}

void DialoguePlayer::advanceLine()
{
    if (!active_) {
        return;
    }

    if (stepIndex_ + 1 >= static_cast<int>(sequence_.steps.size())) {
        closing_ = true;
        revealCurrentLine();
        return;
    }

    ++stepIndex_;
    lineElapsed_ = 0.0f;
    syncRightPortraitForCurrentLine(false);
}

void DialoguePlayer::syncRightPortraitForCurrentLine(bool immediate)
{
    const DialogueLine* line = currentLine();
    if (line == nullptr || line->speakerId.empty() || line->speakerId == "player") {
        return;
    }
    setRightPortraitTarget(line->speakerId, immediate);
}

void DialoguePlayer::setRightPortraitTarget(std::string speakerId, bool immediate)
{
    if (speakerId.empty()) {
        return;
    }
    if (speakerId == rightSpeakerId_) {
        pendingRightSpeakerId_.clear();
        rightPortraitTransition_ = rightPortraitFade_ >= 1.0f
            ? RightPortraitTransition::Stable
            : RightPortraitTransition::FadingIn;
        return;
    }
    if (!pendingRightSpeakerId_.empty() && speakerId == pendingRightSpeakerId_) {
        return;
    }

    if (rightSpeakerId_.empty() || immediate) {
        rightSpeakerId_ = std::move(speakerId);
        pendingRightSpeakerId_.clear();
        rightPortraitFade_ = immediate ? 1.0f : 0.0f;
        rightPortraitTransition_ = immediate
            ? RightPortraitTransition::Stable
            : RightPortraitTransition::FadingIn;
        return;
    }

    pendingRightSpeakerId_ = std::move(speakerId);
    rightPortraitTransition_ = RightPortraitTransition::FadingOut;
}

void DialoguePlayer::updateRightPortrait(float dt)
{
    const float fadeStep = std::max(0.0f, dt) / std::max(0.001f, DialoguePortraitFadeSeconds);
    switch (rightPortraitTransition_) {
    case RightPortraitTransition::Stable:
        return;
    case RightPortraitTransition::FadingOut:
        rightPortraitFade_ = std::max(0.0f, rightPortraitFade_ - fadeStep);
        if (rightPortraitFade_ <= 0.0f) {
            rightSpeakerId_ = std::move(pendingRightSpeakerId_);
            pendingRightSpeakerId_.clear();
            rightPortraitTransition_ = rightSpeakerId_.empty()
                ? RightPortraitTransition::Stable
                : RightPortraitTransition::FadingIn;
        }
        return;
    case RightPortraitTransition::FadingIn:
        rightPortraitFade_ = std::min(1.0f, rightPortraitFade_ + fadeStep);
        if (rightPortraitFade_ >= 1.0f) {
            rightPortraitTransition_ = RightPortraitTransition::Stable;
        }
        return;
    }
}

}
