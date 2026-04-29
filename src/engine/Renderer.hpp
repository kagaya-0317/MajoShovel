#pragma once

#include "engine/Camera.hpp"
#include "engine/Math.hpp"
#include <SDL3/SDL.h>
#include <memory>
#include <string>
#include <string_view>

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

    void fillRect(Vec2 pos, Vec2 size, Color color);
    void drawRect(Vec2 pos, Vec2 size, Color color);
    void fillCircle(Vec2 center, float radius, Color color);
    void drawCircle(Vec2 center, float radius, Color color);
    void drawLine(Vec2 a, Vec2 b, Color color);
    void drawText(Vec2 pos, std::string_view text, Color color, int scale = 2);
    bool loadTextFont(std::string_view path);
    bool loadIconSheet(std::string_view path, int iconSize = 32, int columns = 8, int rows = 8);
    void unloadIconSheet();
    bool hasIconSheet() const { return iconSheet_.texture != nullptr; }
    const std::string& lastAssetError() const { return lastAssetError_; }
    void drawIcon(int index, Vec2 pos, float scale = 1.0f, Color tint = {255, 255, 255, 255});
    void drawIcon(int index, Vec2 pos, Vec2 size, Color tint = {255, 255, 255, 255});

private:
    struct IconSheet {
        SDL_Texture* texture = nullptr;
        int iconSize = 32;
        int columns = 0;
        int rows = 0;
    };

    struct NativeTextFont;

    Vec2 transform(Vec2 p) const;
    void setColor(Color color);
    void drawGlyph(char c, Vec2 pos, Color color, int scale);
    bool drawNativeText(Vec2 pos, std::string_view text, Color color, int scale);

    SDL_Renderer* renderer_ = nullptr;
    const Camera* camera_ = nullptr;
    IconSheet iconSheet_;
    std::unique_ptr<NativeTextFont> nativeTextFont_;
    std::string lastAssetError_;
};

}
