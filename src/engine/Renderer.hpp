#pragma once

#include "engine/Camera.hpp"
#include "engine/Math.hpp"
#include <SDL3/SDL.h>
#include <array>
#include <memory>
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
    void drawLine(Vec2 a, Vec2 b, Color color);
    void drawText(Vec2 pos, std::string_view text, Color color, int scale = 2);
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
    const std::string& lastAssetError() const { return lastAssetError_; }
    void drawIcon(int index, Vec2 pos, float scale = 1.0f, Color tint = {255, 255, 255, 255});
    void drawIcon(int index, Vec2 pos, Vec2 size, Color tint = {255, 255, 255, 255});
    void drawPlayerSprite(int index, Vec2 center, float size, bool flipHorizontal, Color tint = {255, 255, 255, 255});
    void drawUiWindowFrame(Vec2 pos, Vec2 size, Color tint = {255, 255, 255, 255});
    void drawUiSubWindowFrame(Vec2 pos, Vec2 size, Color tint = {255, 255, 255, 255});
    void drawUiButtonFrame(Vec2 pos, float width, int variant, Color tint = {255, 255, 255, 255});

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

    Vec2 transform(Vec2 p) const;
    Vec2 transformSize(Vec2 size) const;
    Color transformColor(Color color) const;
    float screenScale() const;
    void setColor(Color color);
    void drawGlyph(char c, Vec2 pos, Color color, int scale);
    bool drawNativeText(Vec2 pos, std::string_view text, Color color, int scale);
    bool measureNativeText(std::string_view text, int scale, Vec2& outSize);
    bool renderNativeTextToTexture(std::string_view text, Color color, int scale, TextTexture& outTexture);
    void clearTextCache();
    std::string wrappedText(std::string_view text, float maxWidth, int scale);
    bool loadSpriteSheet(std::string_view path, int frameSize, int columns, int rows, std::string_view label, SpriteSheet& sheet);
    void unloadSpriteSheet(SpriteSheet& sheet);
    bool loadGuidedTexture(std::string_view path, int columns, int rows, bool transparentOnly, std::string_view label, GuidedTexture& target);
    void unloadGuidedTexture(GuidedTexture& texture);
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
    GuidedTexture uiWindowTexture_;
    GuidedTexture uiSubWindowTexture_;
    GuidedTexture uiButtonTexture_;
    std::unique_ptr<NativeTextFont> nativeTextFont_;
    std::unordered_map<std::string, TextTexture> textCache_;
    std::unordered_map<std::string, Vec2> textMeasureCache_;
    std::unordered_map<std::string, std::string> wrappedTextCache_;
    std::string lastAssetError_;
};

}
