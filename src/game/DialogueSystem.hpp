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
    bool portraitRight = false;
};

struct DialogueSequence {
    std::string id;
    std::vector<DialogueLine> lines;
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
    [[nodiscard]] const DialogueLine* currentLine() const;
    [[nodiscard]] int currentLineGlyphCount() const;
    [[nodiscard]] float currentLineCompletionTime() const;
    void revealCurrentLine();
    void advanceLine();

    DialogueSequence sequence_;
    int lineIndex_ = 0;
    float openElapsed_ = 0.0f;
    float lineElapsed_ = 0.0f;
    float contentFade_ = 0.0f;
    bool active_ = false;
    bool closing_ = false;
};

}
