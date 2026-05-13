#pragma once

#include "engine/Camera.hpp"
#include "engine/Math.hpp"
#include <SDL3/SDL.h>
#include <array>
#include <cstdint>
#include <memory>
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

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

class Renderer {
public:
    explicit Renderer(SDL_Renderer* renderer);
    ~Renderer();

    void clear(Color color);
    void present();
    void setCamera(const Camera* camera) { camera_ = camera; }
    void setScreenSpace();
    void setWorldSpace(const Camera* camera) { camera_ = camera; }
    void pushScreenTransform(Vec2 origin, float scale, float alpha);
    void popScreenTransform();

    void fillRect(Vec2 pos, Vec2 size, Color color);
    void drawRect(Vec2 pos, Vec2 size, Color color);
    void fillCircle(Vec2 center, float radius, Color color);
    void drawCircle(Vec2 center, float radius, Color color);
    void fillSoftCircle(Vec2 center, float radius, Color color);
    void drawSoftRing(Vec2 center, float radius, float width, Color color);
    void fillEllipse(Vec2 center, Vec2 radius, Color color);
    void drawActorShadow(Vec2 actorAnchor, float visualSize, Color color = {0, 0, 0, 82});
    void drawLine(Vec2 a, Vec2 b, Color color);
    void drawSoftLine(Vec2 a, Vec2 b, float width, Color color);
    void drawSoftPolyline(const std::vector<Vec2>& points, float width, Color color);
    void drawText(Vec2 pos, std::string_view text, Color color, int scale = 2);
    void drawOutlinedText(
        Vec2 pos,
        std::string_view text,
        Color color,
        Color outline,
        int outlinePx,
        int scale = 2);
    Vec2 measureText(std::string_view text, int scale = 2);
    Vec2 measureWrappedText(std::string_view text, float maxWidth, int scale = 2);
    void drawWrappedText(Vec2 pos, std::string_view text, float maxWidth, Color color, int scale = 2);
    bool loadTextFont(std::string_view path);
    bool loadIconSheet(std::string_view path, int iconSize = 32, int columns = 8, int rows = 8);
    void unloadIconSheet();
    bool hasIconSheet() const { return iconSheet_.texture != nullptr; }
    bool loadPlayerSheet(std::string_view path, int frameSize = 96, int columns = 3, int rows = 3);
    void unloadPlayerSheet();
    bool hasPlayerSheet() const { return playerSheet_.texture != nullptr; }
    bool loadUiWindowTexture(std::string_view path);
    void unloadUiWindowTexture();
    bool hasUiWindowTexture() const { return uiWindowTexture_.texture != nullptr && uiWindowTexture_.valid; }
    Vec2 uiWindowMinSize() const;
    bool loadUiSubWindowTexture(std::string_view path);
    void unloadUiSubWindowTexture();
    bool hasUiSubWindowTexture() const { return uiSubWindowTexture_.texture != nullptr && uiSubWindowTexture_.valid; }
    bool loadUiButtonTexture(std::string_view path);
    void unloadUiButtonTexture();
    bool hasUiButtonTexture() const { return uiButtonTexture_.texture != nullptr && uiButtonTexture_.valid; }
    bool loadUiLineTexture(std::string_view path);
    void unloadUiLineTexture();
    bool hasUiLineTexture() const { return uiLineTexture_.texture != nullptr; }
    bool loadBaseMapTexture(std::string_view path);
    void unloadBaseMapTexture();
    bool hasBaseMapTexture() const { return baseMapTexture_.texture != nullptr; }
    const std::string& lastAssetError() const { return lastAssetError_; }
    void drawIcon(int index, Vec2 pos, float scale = 1.0f, Color tint = {255, 255, 255, 255});
    void drawIcon(int index, Vec2 pos, Vec2 size, Color tint = {255, 255, 255, 255});
    void drawPlayerSprite(int index, Vec2 anchorPosition, float size, bool flipHorizontal, Color tint = {255, 255, 255, 255}, Vec2 anchor = {0.5f, 0.82f});
    void drawBaseMapTexture(Vec2 pos, Vec2 size, Color tint = {255, 255, 255, 255});
    void drawUiWindowFrame(Vec2 pos, Vec2 size, Color tint = {255, 255, 255, 255});
    void drawUiSubWindowFrame(Vec2 pos, Vec2 size, Color tint = {255, 255, 255, 255});
    void drawUiButtonFrame(Vec2 pos, float width, int variant, Color tint = {255, 255, 255, 255});
    void drawUiLine(Vec2 pos, float width, Color tint = {255, 255, 255, 255});
    ImageHandle acquireImage(std::string_view path, TextureFilter filter = TextureFilter::Nearest);
    bool drawImage(ImageHandle handle, Vec2 center, Vec2 size, const ImageDrawOptions& options = {});
    bool drawImageRegion(ImageHandle handle, SDL_FRect sourceRect, Vec2 center, Vec2 size, const ImageDrawOptions& options = {});
    bool drawImage(
        std::string_view path,
        Vec2 center,
        Vec2 size,
        const ImageDrawOptions& options = {},
        TextureFilter filter = TextureFilter::Nearest);
    bool getImageSize(ImageHandle handle, Vec2& outSize) const;
    bool getImageSize(std::string_view path, Vec2& outSize, TextureFilter filter = TextureFilter::Nearest);
    void invalidateImage(std::string_view path);
    void invalidateAllImages();
    void setImageCacheBudgetBytes(std::size_t bytes);
    [[nodiscard]] ImageCacheStats imageCacheStats() const;

private:
    struct SpriteSheet {
        SDL_Texture* texture = nullptr;
        int frameSize = 32;
        int columns = 0;
        int rows = 0;
    };

    struct NativeTextFont;
    struct TextTexture {
        SDL_Texture* texture = nullptr;
        int width = 0;
        int height = 0;
    };

    struct GuidedTexture {
        SDL_Texture* texture = nullptr;
        int width = 0;
        int height = 0;
        bool valid = false;
        int columns = 0;
        int rows = 0;
        std::array<SDL_FRect, 15> cells{};
        std::array<float, 5> columnWidths{};
        std::array<float, 3> rowHeights{};
    };

    struct ImageTexture {
        SDL_Texture* texture = nullptr;
        SDL_Texture* outlineTexture = nullptr;
        int width = 0;
        int height = 0;
    };

    struct CachedImageEntry {
        ImageTexture texture{};
        std::string normalizedPath;
        TextureFilter filter = TextureFilter::Nearest;
        std::size_t approxBytes = 0;
        std::uint64_t lastUsedFrame = 0;
        std::uint64_t nextRetryTicks = 0;
        std::uint32_t failureCount = 0;
        bool failed = false;
    };

    Vec2 transform(Vec2 p) const;
    Vec2 transformSize(Vec2 size) const;
    Color transformColor(Color color) const;
    float screenScale() const;
    void setColor(Color color);
    void drawGlyph(char c, Vec2 pos, Color color, int scale);
    bool drawNativeText(Vec2 pos, std::string_view text, Color color, int scale);
    bool drawNativeOutlinedText(Vec2 pos, std::string_view text, Color color, Color outline, int outlinePx, int scale);
    bool measureNativeText(std::string_view text, int scale, Vec2& outSize);
    bool renderNativeTextToTexture(std::string_view text, Color color, int scale, TextTexture& outTexture);
    bool renderNativeOutlinedTextToTexture(
        std::string_view text,
        Color color,
        Color outline,
        int scale,
        int outlinePx,
        TextTexture& outTexture);
    void clearTextCache();
    std::string wrappedText(std::string_view text, float maxWidth, int scale);
    bool loadSpriteSheet(std::string_view path, int frameSize, int columns, int rows, std::string_view label, SpriteSheet& sheet);
    void unloadSpriteSheet(SpriteSheet& sheet);
    bool loadImageTexture(std::string_view path, std::string_view label, ImageTexture& target);
    void unloadImageTexture(ImageTexture& texture);
    bool loadGuidedTexture(std::string_view path, int columns, int rows, bool transparentOnly, std::string_view label, GuidedTexture& target);
    void unloadGuidedTexture(GuidedTexture& texture);
    CachedImageEntry* findImageEntry(ImageHandle handle);
    const CachedImageEntry* findImageEntry(ImageHandle handle) const;
    bool ensureImageReady(CachedImageEntry& entry);
    void touchImage(CachedImageEntry& entry);
    void destroyCachedImageTexture(CachedImageEntry& entry);
    void evictImageCacheIfNeeded();
    void eraseImageHandle(ImageHandle handle);
    void drawTextureRegion(SDL_Texture* texture, SDL_FRect src, Vec2 pos, Vec2 size, Color tint);
    void drawTextureTiled(SDL_Texture* texture, SDL_FRect src, Vec2 pos, Vec2 size, Color tint);
    void drawNineSliceFrame(const GuidedTexture& texture, Vec2 pos, Vec2 size, Color tint);
    void drawHorizontalSliceRow(const GuidedTexture& texture, int row, Vec2 pos, float width, Color tint);

    SDL_Renderer* renderer_ = nullptr;
    const Camera* camera_ = nullptr;
    struct ScreenTransform {
        Vec2 origin{};
        float scale = 1.0f;
        float alpha = 1.0f;
    };
    std::vector<ScreenTransform> screenTransforms_;
    SpriteSheet iconSheet_;
    SpriteSheet playerSheet_;
    ImageTexture baseMapTexture_;
    GuidedTexture uiWindowTexture_;
    GuidedTexture uiSubWindowTexture_;
    GuidedTexture uiButtonTexture_;
    ImageTexture uiLineTexture_;
    std::unique_ptr<NativeTextFont> nativeTextFont_;
    std::unordered_map<std::string, TextTexture> textCache_;
    std::unordered_map<std::string, Vec2> textMeasureCache_;
    std::unordered_map<std::string, std::string> wrappedTextCache_;
    std::unordered_map<std::string, ImageHandle> imageHandleByKey_;
    std::unordered_map<std::uint32_t, CachedImageEntry> imageEntries_;
    std::uint32_t nextImageHandleValue_ = 1;
    std::size_t imageCacheBudgetBytes_ = 64ULL * 1024ULL * 1024ULL;
    std::size_t imageCacheBytes_ = 0;
    std::uint64_t frameCounter_ = 0;
    std::size_t imageCacheHitCount_ = 0;
    std::size_t imageCacheMissCount_ = 0;
    std::size_t imageCacheLoadFailCount_ = 0;
    std::string lastAssetError_;
};

}
