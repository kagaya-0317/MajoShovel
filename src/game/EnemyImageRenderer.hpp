#pragma once

#include "engine/Math.hpp"
#include "engine/Renderer.hpp"
#include "game/ArtworkOutline.hpp"
#include "game/Enemy.hpp"

#include <string>

namespace majo {

struct EnemyImageDrawOptions {
    Vec2 anchor{0.5f, 0.5f};
    Color tint{255, 255, 255, 255};
    TextureFilter filter = TextureFilter::Nearest;
    bool allowUpscale = false;
    bool fitToMaxSize = true;
    Color maskOverlayColor{255, 255, 255, 0};
    bool outlineEnabled = ArtworkOutlineEnabled;
    Color outlineColor{ArtworkOutlineColor};
    int outlinePx = ArtworkOutlinePx;
    float scaleMultiplier = 1.0f;
    Vec2 stretchScale{1.0f, 1.0f};
    float rotationDegrees = 0.0f;
    bool flipX = false;
    bool flipY = false;
    bool directionOverrideEnabled = false;
    Vec2 directionOverride{0.0f, 1.0f};
};

struct EnemyImageDebugInfo {
    bool valid = false;
    const char* directionSource = "none";
    const char* directionName = "none";
    const char* facingDirectionName = "none";
    const char* motionName = "none";
    bool directionOverrideEnabled = false;
    int directionIndex = 0;
    int frameRow = 0;
    int frameColumn = 0;
    int sheetRows = 0;
    int sheetColumns = 0;
    int framesPerAnimation = 0;
    float facingAngleDegrees = 0.0f;
    Vec2 facingVector{};
    Vec2 directionOverride{};
};

[[nodiscard]] std::string enemyImagePathFromNumber(int imageNumber);
[[nodiscard]] std::string enemyImagePath(const EnemyDefinition& enemy);
[[nodiscard]] EnemyImageDebugInfo enemyImageDebugInfo(
    const Enemy& enemy,
    float animationTimeSeconds,
    const EnemyImageDrawOptions& options = {});
[[nodiscard]] Vec2 junkCrabAttackForwardMotionOffset(
    const Enemy& enemy,
    float animationTimeSeconds,
    const EnemyImageDrawOptions& options = {});
[[nodiscard]] Vec2 junkCrabAttackChargeShakeOffset(
    const Enemy& enemy,
    float animationTimeSeconds,
    const EnemyImageDrawOptions& options = {});
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
