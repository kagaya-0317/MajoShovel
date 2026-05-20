#pragma once

#include "engine/Math.hpp"
#include "engine/Renderer.hpp"
#include "game/Enemy.hpp"

#include <string>

namespace majo {

struct EnemyImageDrawOptions {
    Vec2 anchor{0.5f, 0.5f};
    Color tint{255, 255, 255, 255};
    TextureFilter filter = TextureFilter::Nearest;
    bool allowUpscale = false;
    bool outlineEnabled = true;
    Color outlineColor{80, 18, 28, 255};
    int outlinePx = 1;
    bool selectedOutlineEnabled = false;
    Color selectedOutlineColor{255, 230, 150, 255};
    int selectedOutlinePx = 6;
    Color maskOverlayColor{255, 255, 255, 0};
    float scaleMultiplier = 1.0f;
    float rotationDegrees = 0.0f;
    bool flipX = false;
    bool flipY = false;
    bool directionOverrideEnabled = false;
    Vec2 directionOverride{0.0f, 1.0f};
};

[[nodiscard]] std::string enemyImagePathFromNumber(int imageNumber);
[[nodiscard]] std::string enemyImagePath(const EnemyDefinition& enemy);
[[nodiscard]] bool enemyImageDrawSize(
    Renderer& renderer,
    const Enemy& enemy,
    const EnemyImageDrawOptions& options,
    Vec2& outDrawSize);
[[nodiscard]] bool drawEnemyImage(
    Renderer& renderer,
    const Enemy& enemy,
    Vec2 center,
    float animationTimeSeconds,
    const EnemyImageDrawOptions& options = {},
    Vec2* outDrawSize = nullptr);
[[nodiscard]] bool drawEnemyImageIcon(
    Renderer& renderer,
    int imageNumber,
    Vec2 center,
    Vec2 maxSize,
    float animationTimeSeconds = 0.0f,
    const EnemyImageDrawOptions& options = {},
    Vec2* outDrawSize = nullptr);

}
