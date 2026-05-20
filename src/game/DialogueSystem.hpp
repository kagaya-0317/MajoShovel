#pragma once

#include "engine/Input.hpp"
#include "engine/Renderer.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace majo {

struct DialogueLine {
    std::string speakerId;
    std::string speakerName;
    std::string portraitPath;
    std::string text;
};

enum class DialogueStepKind {
    Line,
    Wait,
};

struct DialogueStep {
    DialogueStepKind kind = DialogueStepKind::Line;
    DialogueLine line;
    float waitSeconds = 0.0f;
};

struct DialogueSequence {
    std::string id;
    std::vector<DialogueLine> lines;
    std::vector<DialogueStep> steps;
};

class DialoguePlayer {
public:
    void start(DialogueSequence sequence);
    void clear();
    void update(const Input& input, float dt);
    void render(Renderer& renderer, int screenWidth, int screenHeight) const;

    [[nodiscard]] bool active() const { return active_; }
    [[nodiscard]] bool lineComplete() const;

private:
    enum class RightPortraitTransition {
        Stable,
        FadingOut,
        FadingIn,
    };

    [[nodiscard]] const DialogueStep* currentStep() const;
    [[nodiscard]] const DialogueLine* currentLine() const;
    [[nodiscard]] int currentLineGlyphCount() const;
    [[nodiscard]] float currentLineCompletionTime() const;
    [[nodiscard]] bool spokenLineSeen() const;
    [[nodiscard]] bool advanceRequested(const Input& input, float dt);
    void revealCurrentLine();
    void advanceLine();
    void resetAdvanceHoldRepeat();
    void syncRightPortraitForCurrentLine(bool immediate);
    void setRightPortraitTarget(std::string speakerId, bool immediate);
    void updateRightPortrait(float dt);
    void renderMonicaCall(Renderer& renderer, int screenWidth, int screenHeight, const DialogueLine* line) const;

    DialogueSequence sequence_;
    int stepIndex_ = 0;
    float openElapsed_ = 0.0f;
    float lineElapsed_ = 0.0f;
    float contentFade_ = 0.0f;
    float advanceRepeatTimer_ = 0.0f;
    std::string rightSpeakerId_;
    std::string pendingRightSpeakerId_;
    float rightPortraitFade_ = 0.0f;
    RightPortraitTransition rightPortraitTransition_ = RightPortraitTransition::Stable;
    bool active_ = false;
    bool closing_ = false;
    bool advanceHoldActive_ = false;
};

}
