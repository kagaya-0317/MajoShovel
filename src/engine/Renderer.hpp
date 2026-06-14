#pragma once

#include "engine/Camera.hpp"
#include "engine/Math.hpp"
#include "engine/RendererTypes.hpp"
#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace majo {

enum class TextFontRole {
    Ui,
    InputGlyph,
};

class Renderer {
public:
    struct ScreenshotResult {
        bool success = false;
        std::filesystem::path path;
        std::string message;
    };

    explicit Renderer(SDL_Renderer* renderer);
    ~Renderer();

    void clear(Color color);
    void present();
    void setScreenBrightness(float brightness);
    bool setLogicalPresentation(
        int width,
        int height,
        LogicalPresentationMode mode = LogicalPresentationMode::Letterbox);
    Vec2 windowToRenderCoordinates(Vec2 windowPosition) const;
    bool convertEventToRenderCoordinates(SDL_Event& event) const;
    void requestScreenshot(std::filesystem::path path);
    std::optional<ScreenshotResult> consumeScreenshotResult();
    void setCamera(const Camera* camera) { camera_ = camera; worldScreenOffset_ = {}; }
    void setScreenSpace();
    void setWorldSpace(const Camera* camera, Vec2 screenOffset = {}) { camera_ = camera; worldScreenOffset_ = screenOffset; }
    void pushScreenTransform(Vec2 origin, float scale, float alpha);
    void popScreenTransform();
    void pushClipRect(Vec2 pos, Vec2 size);
    void popClipRect();

    void fillRect(Vec2 pos, Vec2 size, Color color);
    void fillGradientRect(
        Vec2 pos,
        Vec2 size,
        Color startColor,
        Color endColor,
        GradientDirection direction = GradientDirection::LeftToRight);
    void fillGradientRect(Vec2 pos, Vec2 size, Color topLeft, Color topRight, Color bottomRight, Color bottomLeft);
    void drawRect(Vec2 pos, Vec2 size, Color color);
    void fillCircle(Vec2 center, float radius, Color color);
    void drawCircle(Vec2 center, float radius, Color color);
    void fillPolygon(const Vec2* points, std::size_t count, Color color);
    void fillTriangleList(const Vec2* vertices, std::size_t vertexCount, const int* indices, std::size_t indexCount, Color color);
    bool drawImageTriangleList(
        ImageHandle handle,
        const ImageTriangleVertex* vertices,
        std::size_t vertexCount,
        const int* indices,
        std::size_t indexCount,
        Color tint = {255, 255, 255, 255});
    void fillSoftCircle(Vec2 center, float radius, Color color);
    void drawSoftRing(Vec2 center, float radius, float width, Color color);
    void fillSoftRingArc(
        Vec2 center,
        float innerRadius,
        float outerRadius,
        float startAngle,
        float sweepAngle,
        Color startColor,
        Color endColor);
    void fillEllipse(Vec2 center, Vec2 radius, Color color);
    void drawActorShadow(Vec2 actorAnchor, float visualSize, Color color = {0, 0, 0, 82});
    void drawActorShadow(Vec2 actorAnchor, float visualSize, Vec2 scale, Color color = {0, 0, 0, 82});
    void drawLine(Vec2 a, Vec2 b, Color color);
    void drawSoftLine(Vec2 a, Vec2 b, float width, Color color);
    void drawSoftPolyline(const std::vector<Vec2>& points, float width, Color color);
    void drawText(Vec2 pos, std::string_view text, Color color, int scale = 2, TextStyle style = TextStyle::Regular);
    void drawText(
        Vec2 pos,
        std::string_view text,
        Color color,
        int scale,
        TextStyle style,
        TextFontRole fontRole);
    void drawOutlinedText(
        Vec2 pos,
        std::string_view text,
        Color color,
        Color outline,
        int outlinePx,
        int scale = 2,
        TextStyle style = TextStyle::Regular);
    Vec2 measureText(std::string_view text, int scale = 2, TextStyle style = TextStyle::Regular);
    Vec2 measureText(
        std::string_view text,
        int scale,
        TextStyle style,
        TextFontRole fontRole);
    Vec2 measureWrappedText(std::string_view text, float maxWidth, int scale = 2, TextStyle style = TextStyle::Regular);
    void drawWrappedText(Vec2 pos, std::string_view text, float maxWidth, Color color, int scale = 2, TextStyle style = TextStyle::Regular);
    bool loadTextFont(std::string_view path, TextFontRole fontRole = TextFontRole::Ui);
    bool loadPlayerSheet(std::string_view path, int frameSize = 0, int columns = 3, int rows = 4);
    bool loadPlayerHandSheet(std::string_view path, int frameSize = 0, int columns = 3, int rows = 4);
    void unloadPlayerSheet();
    void unloadPlayerHandSheet();
    bool hasPlayerSheet() const { return playerSheet_.texture != nullptr; }
    bool hasPlayerHandSheet() const { return playerHandSheet_.texture != nullptr; }
    Vec2 playerSpriteFrameSize() const;
    bool loadUiWindowTexture(std::string_view path);
    void unloadUiWindowTexture();
    bool hasUiWindowTexture() const { return uiWindowTexture_.texture != nullptr && uiWindowTexture_.valid; }
    Vec2 uiWindowMinSize() const;
    bool loadUiMessageWindowTexture(std::string_view path);
    void unloadUiMessageWindowTexture();
    bool hasUiMessageWindowTexture() const { return uiMessageWindowTexture_.texture != nullptr; }
    Vec2 uiMessageWindowSize() const;
    bool loadUiSubWindowTexture(std::string_view path);
    void unloadUiSubWindowTexture();
    bool hasUiSubWindowTexture() const { return uiSubWindowTexture_.texture != nullptr && uiSubWindowTexture_.valid; }
    bool loadUiButtonTexture(std::string_view path);
    void unloadUiButtonTexture();
    bool hasUiButtonTexture() const { return uiButtonTexture_.texture != nullptr && uiButtonTexture_.valid; }
    bool loadUiTabTexture(std::string_view path);
    void unloadUiTabTexture();
    bool hasUiTabTexture() const { return uiTabTexture_.texture != nullptr && uiTabTexture_.valid; }
    bool loadUiHorizontalTabTexture(std::string_view path);
    void unloadUiHorizontalTabTexture();
    bool hasUiHorizontalTabTexture() const { return uiHorizontalTabTexture_.texture != nullptr && uiHorizontalTabTexture_.valid; }
    bool loadUiLineTexture(std::string_view path);
    void unloadUiLineTexture();
    bool hasUiLineTexture() const { return uiLineTexture_.texture != nullptr; }
    bool loadBaseMapTexture(std::string_view path);
    void unloadBaseMapTexture();
    bool hasBaseMapTexture() const { return baseMapTexture_.texture != nullptr; }
    const std::string& lastAssetError() const { return lastAssetError_; }
    void drawPlayerSprite(
        int index,
        Vec2 anchorPosition,
        float size,
        bool flipHorizontal,
        Color tint = {255, 255, 255, 255},
        Vec2 anchor = {0.5f, 0.82f},
        bool flipVertical = false);
    void drawPlayerSpriteNaturalSize(
        int index,
        Vec2 anchorPosition,
        float scale,
        bool flipHorizontal,
        Color tint = {255, 255, 255, 255},
        Vec2 anchor = {0.5f, 0.82f},
        bool flipVertical = false);
    void drawPlayerHandSpriteNaturalSize(
        int index,
        Vec2 anchorPosition,
        float scale,
        bool flipHorizontal,
        Color tint = {255, 255, 255, 255},
        Vec2 anchor = {0.5f, 0.82f},
        bool flipVertical = false);
    void drawBaseMapTexture(Vec2 pos, Vec2 size, Color tint = {255, 255, 255, 255});
    FrameSnapshot captureFrameSnapshot();
    void destroyFrameSnapshot(FrameSnapshot& snapshot);
    bool drawFrameSnapshot(const FrameSnapshot& snapshot, Vec2 pos, Vec2 size, Color tint = {255, 255, 255, 255});
    void drawUiWindowFrame(Vec2 pos, Vec2 size, Color tint = {255, 255, 255, 255});
    void drawUiMessageWindowFrame(Vec2 pos, Vec2 size, Color tint = {255, 255, 255, 255});
    void drawUiSubWindowFrame(Vec2 pos, Vec2 size, Color tint = {255, 255, 255, 255});
    void drawUiButtonFrame(Vec2 pos, float width, int variant, Color tint = {255, 255, 255, 255});
    void drawUiTabFrame(Vec2 pos, Vec2 size, bool selected, Color tint = {255, 255, 255, 255});
    void drawUiHorizontalTabs(
        const Vec2* positions,
        const Vec2* sizes,
        const int* selected,
        const Color* tints,
        int count);
    void drawUiLine(Vec2 pos, float width, Color tint = {255, 255, 255, 255});
    ImageHandle acquireImage(std::string_view path, TextureFilter filter = TextureFilter::Nearest);
    bool drawImage(ImageHandle handle, Vec2 center, Vec2 size, const ImageDrawOptions& options = {});
    bool drawImageRegion(ImageHandle handle, RectF sourceRect, Vec2 center, Vec2 size, const ImageDrawOptions& options = {});
    bool drawImageHorizontalSlices(
        ImageHandle handle,
        RectF sourceRect,
        Vec2 pos,
        Vec2 size,
        float leftWidth,
        float rightWidth,
        const ImageDrawOptions& options = {});
    bool drawImage(
        std::string_view path,
        Vec2 center,
        Vec2 size,
        const ImageDrawOptions& options = {},
        TextureFilter filter = TextureFilter::Nearest);
    bool getImageSize(ImageHandle handle, Vec2& outSize) const;
    bool getImageSize(std::string_view path, Vec2& outSize, TextureFilter filter = TextureFilter::Nearest);
    bool imageHitTestAlpha(
        ImageHandle handle,
        Vec2 center,
        Vec2 size,
        Vec2 point,
        const ImageDrawOptions& options = {},
        unsigned char alphaThreshold = 1);
    void invalidateImage(std::string_view path);
    void invalidateAllImages();
    void setImageCacheBudgetBytes(std::size_t bytes);
    [[nodiscard]] ImageCacheStats imageCacheStats() const;

private:
    struct SpriteSheet {
        SDL_Texture* texture = nullptr;
        int frameWidth = 32;
        int frameHeight = 32;
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
        std::array<RectF, 15> cells{};
        std::array<float, 5> columnWidths{};
        std::array<float, 3> rowHeights{};
        std::array<float, 15> contentLeftInsets{};
        std::array<float, 15> contentRightInsets{};
    };

    struct ImageTexture {
        SDL_Texture* texture = nullptr;
        SDL_Texture* outlineTexture = nullptr;
        int width = 0;
        int height = 0;
        std::vector<unsigned char> alphaMask;
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
    void applyClipRect();
    void applyScreenBrightnessOverlay();
    void setColor(Color color);
    void drawGlyph(char c, Vec2 pos, Color color, int scale);
    bool drawNativeText(Vec2 pos, std::string_view text, Color color, int scale, TextStyle style, TextFontRole fontRole);
    bool drawNativeOutlinedText(Vec2 pos, std::string_view text, Color color, Color outline, int outlinePx, int scale, TextStyle style);
    bool measureNativeText(std::string_view text, int scale, TextStyle style, TextFontRole fontRole, Vec2& outSize);
    bool renderNativeTextToTexture(std::string_view text, Color color, int scale, TextStyle style, TextFontRole fontRole, TextTexture& outTexture);
    bool renderNativeOutlinedTextToTexture(
        std::string_view text,
        Color color,
        Color outline,
        int scale,
        int outlinePx,
        TextStyle style,
        TextTexture& outTexture);
    void clearTextCache();
    std::string wrappedText(std::string_view text, float maxWidth, int scale, TextStyle style);
    bool loadSpriteSheet(std::string_view path, int frameSize, int columns, int rows, std::string_view label, SpriteSheet& sheet);
    void unloadSpriteSheet(SpriteSheet& sheet);
    bool loadImageTexture(std::string_view path, std::string_view label, ImageTexture& target);
    void unloadImageTexture(ImageTexture& texture);
    bool loadGuidedTexture(
        std::string_view path,
        int columns,
        int rows,
        bool transparentOnly,
        std::string_view label,
        GuidedTexture& target,
        bool useEqualRows = false);
    void unloadGuidedTexture(GuidedTexture& texture);
    CachedImageEntry* findImageEntry(ImageHandle handle);
    const CachedImageEntry* findImageEntry(ImageHandle handle) const;
    std::size_t imageTextureApproxBytes(const ImageTexture& texture) const;
    bool ensureImageReady(CachedImageEntry& entry);
    bool ensureCachedImageOutlineReady(CachedImageEntry& entry);
    void touchImage(CachedImageEntry& entry);
    void destroyCachedImageTexture(CachedImageEntry& entry);
    void evictImageCacheIfNeeded(const CachedImageEntry* protectedEntry = nullptr);
    void eraseImageHandle(ImageHandle handle);
    void drawTextureRegion(SDL_Texture* texture, RectF src, Vec2 pos, Vec2 size, Color tint, bool flipHorizontal = false);
    void drawTextureTiled(SDL_Texture* texture, RectF src, Vec2 pos, Vec2 size, Color tint);
    void drawNineSliceFrame(const GuidedTexture& texture, Vec2 pos, Vec2 size, Color tint);
    void drawHorizontalSliceRow(const GuidedTexture& texture, int row, Vec2 pos, float width, Color tint);
    void drawHorizontalSliceRow(const GuidedTexture& texture, int row, Vec2 pos, Vec2 size, Color tint);
    ScreenshotResult saveCurrentFramePng(const std::filesystem::path& path);

    SDL_Renderer* renderer_ = nullptr;
    const Camera* camera_ = nullptr;
    Vec2 worldScreenOffset_{};
    float screenBrightness_ = 1.0f;
    struct ScreenTransform {
        Vec2 origin{};
        float scale = 1.0f;
        float alpha = 1.0f;
    };
    std::vector<ScreenTransform> screenTransforms_;
    std::vector<RectF> clipStack_;
    SpriteSheet playerSheet_;
    SpriteSheet playerHandSheet_;
    ImageTexture baseMapTexture_;
    GuidedTexture uiWindowTexture_;
    ImageTexture uiMessageWindowTexture_;
    GuidedTexture uiSubWindowTexture_;
    GuidedTexture uiButtonTexture_;
    GuidedTexture uiTabTexture_;
    GuidedTexture uiHorizontalTabTexture_;
    ImageTexture uiLineTexture_;
    std::unique_ptr<NativeTextFont> nativeTextFont_;
    std::unique_ptr<NativeTextFont> inputGlyphTextFont_;
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
    int logicalPresentationWidth_ = 0;
    int logicalPresentationHeight_ = 0;
    LogicalPresentationMode logicalPresentationMode_ = LogicalPresentationMode::Disabled;
    std::optional<std::filesystem::path> pendingScreenshotPath_;
    std::optional<ScreenshotResult> lastScreenshotResult_;
    std::string lastAssetError_;
};

}
