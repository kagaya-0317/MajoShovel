#pragma once

#include "engine/Input.hpp"
#include "engine/Renderer.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace majo {

struct DialogueLine {
    std::string speakerId;
    std::string speakerName;
    std::string portraitPath;
    std::string text;
    std::string sourcePath;
    int sourceCommandLineNumber = 0;
    bool forcePortraitsBright = false;
    std::vector<std::string> brightPortraitSpeakerIds;
};

enum class DialogueStepKind {
    Line,
    Wait,
    PortraitHide,
    PortraitHideSpeaker,
    PortraitExpression,
    Command,
};

struct DialogueCommand {
    std::string name;
    std::vector<std::string> args;
};

struct DialogueStep {
    DialogueStepKind kind = DialogueStepKind::Line;
    DialogueLine line;
    DialogueCommand command;
    std::string portraitSpeakerId;
    std::string portraitExpressionSpeakerId;
    int portraitExpressionVariant = 0;
    float waitSeconds = 0.0f;
    bool persistPortraitHide = false;
};

struct DialogueSequence {
    std::string id;
    std::vector<DialogueLine> lines;
    std::vector<DialogueStep> steps;
};

struct DialogueVisiblePortrait {
    bool visible = false;
    std::string speakerId;
    int expressionVariant = 0;
};

class DialoguePlayer {
public:
    void start(DialogueSequence sequence);
    void clear();
    void update(const Input& input, float dt);
    void render(Renderer& renderer, int screenWidth, int screenHeight) const;
    void completeCurrentCommandStep();

    [[nodiscard]] bool active() const { return active_; }
    [[nodiscard]] bool lineComplete() const;
    [[nodiscard]] int currentStepIndex() const { return stepIndex_; }
    [[nodiscard]] std::string_view currentSpeakerId() const;
    [[nodiscard]] const DialogueCommand* currentCommand() const;
    [[nodiscard]] const DialogueLine* currentEditableLine() const;
    [[nodiscard]] DialogueVisiblePortrait leftPortrait() const;
    [[nodiscard]] DialogueVisiblePortrait rightPortrait() const;
    [[nodiscard]] int portraitExpressionVariant(std::string_view speakerId) const;
    void setPortraitExpressionVariant(std::string speakerId, int variant);
    int consumeAdvanceSoundRequests();

private:
    enum class RightPortraitTransition {
        Stable,
        FadingOut,
        FadingIn,
    };

    struct SpeakingPortraitMotion {
        enum class Cue {
            Neutral,
            Speech,
            Pause,
        };

        enum class Phase {
            WaitingForPortrait,
            WaitingForCue,
            Bouncing,
            FinishingBounceForPause,
            Pausing,
        };

        std::string speakerId;
        std::vector<Cue> cues;
        std::size_t nextCueIndex = 0;
        float cycleElapsedSeconds = 0.0f;
        float pauseRemainingSeconds = 0.0f;
        Phase phase = Phase::WaitingForPortrait;
        bool active = false;
        bool completedCycle = false;
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
    void clearRightPortraitTarget(bool immediate);
    void updateRightPortrait(float dt);
    void applyPortraitExpressionStep(const DialogueStep& step);
    void resetSpeakingPortraitMotion();
    void finishSpeakingPortraitMotion();
    void updateSpeakingPortraitMotion(float dt);
    [[nodiscard]] std::size_t revealedSpeakingPortraitCueCount() const;
    [[nodiscard]] bool speakingPortraitReady() const;
    [[nodiscard]] bool speakingPortraitMotionBlocksAdvance() const;
    [[nodiscard]] float speakingPortraitOffsetY(std::string_view speakerId) const;
    void renderMonicaCall(Renderer& renderer, int screenWidth, int screenHeight, const DialogueLine* line) const;

    DialogueSequence sequence_;
    int stepIndex_ = 0;
    float openElapsed_ = 0.0f;
    float lineElapsed_ = 0.0f;
    float contentFade_ = 0.0f;
    float advanceRepeatTimer_ = 0.0f;
    int advanceSoundRequests_ = 0;
    std::string rightSpeakerId_;
    std::string pendingRightSpeakerId_;
    std::vector<std::pair<std::string, int>> portraitExpressionVariants_;
    float rightPortraitFade_ = 0.0f;
    RightPortraitTransition rightPortraitTransition_ = RightPortraitTransition::Stable;
    SpeakingPortraitMotion speakingPortraitMotion_;
    bool active_ = false;
    bool closing_ = false;
    bool portraitsHidden_ = false;
    bool portraitsHidePersistent_ = false;
    bool advanceHoldActive_ = false;
    bool advanceAfterSpeakingPortraitMotion_ = false;
};

}
