#pragma once

#include "engine/Math.hpp"

#include <cstddef>
#include <cstdint>

struct SDL_Renderer;
struct SDL_Texture;

namespace majo {

struct Color {
    unsigned char r = 255;
    unsigned char g = 255;
    unsigned char b = 255;
    unsigned char a = 255;
};

enum class TextureFilter {
    Nearest,
    Linear,
};

enum class TextStyle {
    Regular,
    Italic,
};

enum class GradientDirection {
    LeftToRight,
    TopToBottom,
    TopLeftToBottomRight,
    BottomLeftToTopRight,
};

struct RectF {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

struct ImageHandle {
    std::uint32_t value = 0;

    [[nodiscard]] bool valid() const { return value != 0; }
    bool operator==(const ImageHandle&) const = default;
};

struct ImageDrawOptions {
    Vec2 anchor{0.5f, 0.5f};
    Color tint{255, 255, 255, 255};
    bool outlineEnabled = false;
    Color outlineColor{0, 0, 0, 255};
    int outlinePx = 1;
    Color maskOverlayColor{255, 255, 255, 0};
    float rotationDegrees = 0.0f;
    bool flipX = false;
    bool flipY = false;
};

struct ImageCacheStats {
    std::size_t textureCount = 0;
    std::size_t totalBytes = 0;
    std::size_t hitCount = 0;
    std::size_t missCount = 0;
    std::size_t loadFailCount = 0;
};

struct FrameSnapshot {
    SDL_Texture* texture = nullptr;
    int width = 0;
    int height = 0;

    [[nodiscard]] bool valid() const { return texture != nullptr; }
};

}
