#pragma once

#include "engine/Math.hpp"
#include "engine/Renderer.hpp"
#include "game/Enemy.hpp"

#include <string>

namespace majo {

struct EnemyImageDrawOptions {
    Color tint{255, 255, 255, 255};
    TextureFilter filter = TextureFilter::Nearest;
    bool outlineEnabled = true;
    Color outlineColor{80, 18, 28, 255};
    int outlinePx = 1;
    Color maskOverlayColor{255, 255, 255, 0};
    float scaleMultiplier = 1.0f;
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

}
