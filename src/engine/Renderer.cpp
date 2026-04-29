#include "engine/Renderer.hpp"

#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
#endif

namespace majo {

namespace {

#ifdef _WIN32
class GdiPlusSession {
public:
    GdiPlusSession()
    {
        Gdiplus::GdiplusStartupInput input;
        initialized_ = Gdiplus::GdiplusStartup(&token_, &input, nullptr) == Gdiplus::Ok;
    }

    ~GdiPlusSession()
    {
        if (initialized_) {
            Gdiplus::GdiplusShutdown(token_);
        }
    }

    bool initialized() const { return initialized_; }

private:
    ULONG_PTR token_ = 0;
    bool initialized_ = false;
};

bool utf8ToWide(std::string_view text, std::wstring& out)
{
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) {
        return false;
    }
    out.resize(static_cast<size_t>(size));
    return MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), size) == size;
}

#endif

}

struct Renderer::NativeTextFont {
#ifdef _WIN32
    Gdiplus::PrivateFontCollection collection;
    std::wstring familyName;
#endif
    bool loaded = false;
};

Renderer::Renderer(SDL_Renderer* renderer) : renderer_(renderer)
{
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
}

Renderer::~Renderer()
{
    unloadIconSheet();
}

void Renderer::setColor(Color color)
{
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
}

Vec2 Renderer::transform(Vec2 p) const
{
    return camera_ ? camera_->worldToScreen(p) : p;
}

void Renderer::clear(Color color)
{
    setColor(color);
    SDL_RenderClear(renderer_);
}

void Renderer::present()
{
    SDL_RenderPresent(renderer_);
}

void Renderer::setScreenSpace()
{
    camera_ = nullptr;
}

void Renderer::fillRect(Vec2 pos, Vec2 size, Color color)
{
    const Vec2 p = transform(pos);
    SDL_FRect rect{p.x, p.y, size.x, size.y};
    setColor(color);
    SDL_RenderFillRect(renderer_, &rect);
}

void Renderer::drawRect(Vec2 pos, Vec2 size, Color color)
{
    const Vec2 p = transform(pos);
    SDL_FRect rect{p.x, p.y, size.x, size.y};
    setColor(color);
    SDL_RenderRect(renderer_, &rect);
}

void Renderer::fillCircle(Vec2 center, float radius, Color color)
{
    const Vec2 c = transform(center);
    setColor(color);
    for (int y = static_cast<int>(-radius); y <= static_cast<int>(radius); ++y) {
        const float span = std::sqrt(std::max(0.0f, radius * radius - static_cast<float>(y * y)));
        SDL_RenderLine(renderer_, c.x - span, c.y + y, c.x + span, c.y + y);
    }
}

void Renderer::drawCircle(Vec2 center, float radius, Color color)
{
    const Vec2 c = transform(center);
    setColor(color);
    constexpr int Segments = 64;
    Vec2 prev{c.x + radius, c.y};
    for (int i = 1; i <= Segments; ++i) {
        const float a = (static_cast<float>(i) / Segments) * Pi * 2.0f;
        Vec2 next{c.x + std::cos(a) * radius, c.y + std::sin(a) * radius};
        SDL_RenderLine(renderer_, prev.x, prev.y, next.x, next.y);
        prev = next;
    }
}

void Renderer::drawLine(Vec2 a, Vec2 b, Color color)
{
    const Vec2 aa = transform(a);
    const Vec2 bb = transform(b);
    setColor(color);
    SDL_RenderLine(renderer_, aa.x, aa.y, bb.x, bb.y);
}

static std::array<unsigned char, 7> glyphRows(char c)
{
    switch (static_cast<char>(std::toupper(static_cast<unsigned char>(c)))) {
    case '0': return {0x0e,0x11,0x13,0x15,0x19,0x11,0x0e};
    case '1': return {0x04,0x0c,0x04,0x04,0x04,0x04,0x0e};
    case '2': return {0x0e,0x11,0x01,0x02,0x04,0x08,0x1f};
    case '3': return {0x1e,0x01,0x01,0x0e,0x01,0x01,0x1e};
    case '4': return {0x02,0x06,0x0a,0x12,0x1f,0x02,0x02};
    case '5': return {0x1f,0x10,0x10,0x1e,0x01,0x01,0x1e};
    case '6': return {0x0e,0x10,0x10,0x1e,0x11,0x11,0x0e};
    case '7': return {0x1f,0x01,0x02,0x04,0x08,0x08,0x08};
    case '8': return {0x0e,0x11,0x11,0x0e,0x11,0x11,0x0e};
    case '9': return {0x0e,0x11,0x11,0x0f,0x01,0x01,0x0e};
    case 'A': return {0x0e,0x11,0x11,0x1f,0x11,0x11,0x11};
    case 'B': return {0x1e,0x11,0x11,0x1e,0x11,0x11,0x1e};
    case 'C': return {0x0e,0x11,0x10,0x10,0x10,0x11,0x0e};
    case 'D': return {0x1e,0x11,0x11,0x11,0x11,0x11,0x1e};
    case 'E': return {0x1f,0x10,0x10,0x1e,0x10,0x10,0x1f};
    case 'F': return {0x1f,0x10,0x10,0x1e,0x10,0x10,0x10};
    case 'G': return {0x0e,0x11,0x10,0x17,0x11,0x11,0x0f};
    case 'H': return {0x11,0x11,0x11,0x1f,0x11,0x11,0x11};
    case 'I': return {0x0e,0x04,0x04,0x04,0x04,0x04,0x0e};
    case 'J': return {0x07,0x02,0x02,0x02,0x12,0x12,0x0c};
    case 'K': return {0x11,0x12,0x14,0x18,0x14,0x12,0x11};
    case 'L': return {0x10,0x10,0x10,0x10,0x10,0x10,0x1f};
    case 'M': return {0x11,0x1b,0x15,0x15,0x11,0x11,0x11};
    case 'N': return {0x11,0x19,0x15,0x13,0x11,0x11,0x11};
    case 'O': return {0x0e,0x11,0x11,0x11,0x11,0x11,0x0e};
    case 'P': return {0x1e,0x11,0x11,0x1e,0x10,0x10,0x10};
    case 'Q': return {0x0e,0x11,0x11,0x11,0x15,0x12,0x0d};
    case 'R': return {0x1e,0x11,0x11,0x1e,0x14,0x12,0x11};
    case 'S': return {0x0f,0x10,0x10,0x0e,0x01,0x01,0x1e};
    case 'T': return {0x1f,0x04,0x04,0x04,0x04,0x04,0x04};
    case 'U': return {0x11,0x11,0x11,0x11,0x11,0x11,0x0e};
    case 'V': return {0x11,0x11,0x11,0x11,0x0a,0x0a,0x04};
    case 'W': return {0x11,0x11,0x11,0x15,0x15,0x1b,0x11};
    case 'X': return {0x11,0x11,0x0a,0x04,0x0a,0x11,0x11};
    case 'Y': return {0x11,0x11,0x0a,0x04,0x04,0x04,0x04};
    case 'Z': return {0x1f,0x01,0x02,0x04,0x08,0x10,0x1f};
    case '+': return {0x00,0x04,0x04,0x1f,0x04,0x04,0x00};
    case '-': return {0x00,0x00,0x00,0x1f,0x00,0x00,0x00};
    case '.': return {0x00,0x00,0x00,0x00,0x00,0x0c,0x0c};
    case ':': return {0x00,0x0c,0x0c,0x00,0x0c,0x0c,0x00};
    case '/': return {0x01,0x01,0x02,0x04,0x08,0x10,0x10};
    case '%': return {0x19,0x19,0x02,0x04,0x08,0x13,0x13};
    default: return {0,0,0,0,0,0,0};
    }
}

void Renderer::drawGlyph(char c, Vec2 pos, Color color, int scale)
{
    const auto rows = glyphRows(c);
    setColor(color);
    for (int y = 0; y < 7; ++y) {
        for (int x = 0; x < 5; ++x) {
            if ((rows[y] >> (4 - x)) & 1U) {
                SDL_FRect rect{pos.x + x * static_cast<float>(scale), pos.y + y * static_cast<float>(scale), static_cast<float>(scale), static_cast<float>(scale)};
                SDL_RenderFillRect(renderer_, &rect);
            }
        }
    }
}

void Renderer::drawText(Vec2 pos, std::string_view text, Color color, int scale)
{
#ifdef _WIN32
    if (drawNativeText(pos, text, color, scale)) {
        return;
    }
#endif

    const Camera* old = camera_;
    camera_ = nullptr;
    Vec2 cursor = pos;
    for (char c : text) {
        if (c == '\n') {
            cursor.x = pos.x;
            cursor.y += 9.0f * scale;
            continue;
        }
        drawGlyph(c, cursor, color, scale);
        cursor.x += 6.0f * scale;
    }
    camera_ = old;
}

bool Renderer::loadTextFont(std::string_view path)
{
    lastAssetError_.clear();
#ifdef _WIN32
    static GdiPlusSession gdiPlus;
    if (!gdiPlus.initialized()) {
        lastAssetError_ = "GDI+ startup failed";
        return false;
    }

    std::wstring widePath;
    if (!utf8ToWide(path, widePath)) {
        lastAssetError_ = "Invalid UTF-8 font path: " + std::string(path);
        return false;
    }

    auto font = std::make_unique<NativeTextFont>();
    if (font->collection.AddFontFile(widePath.c_str()) != Gdiplus::Ok) {
        lastAssetError_ = "Failed to load text font: " + std::string(path);
        return false;
    }

    const int familyCount = font->collection.GetFamilyCount();
    if (familyCount <= 0) {
        lastAssetError_ = "Text font has no usable families: " + std::string(path);
        return false;
    }

    std::vector<Gdiplus::FontFamily> families(static_cast<std::size_t>(familyCount));
    int foundCount = 0;
    if (font->collection.GetFamilies(familyCount, families.data(), &foundCount) != Gdiplus::Ok || foundCount <= 0) {
        lastAssetError_ = "Failed to read text font families: " + std::string(path);
        return false;
    }

    WCHAR familyName[LF_FACESIZE]{};
    if (families[0].GetFamilyName(familyName) != Gdiplus::Ok) {
        lastAssetError_ = "Failed to read text font name: " + std::string(path);
        return false;
    }

    font->familyName = familyName;
    font->loaded = true;
    nativeTextFont_ = std::move(font);
    return true;
#else
    lastAssetError_ = "TTF text rendering is only implemented on Windows";
    return false;
#endif
}

bool Renderer::drawNativeText(Vec2 pos, std::string_view text, Color color, int scale)
{
#ifdef _WIN32
    if (!nativeTextFont_ || !nativeTextFont_->loaded) {
        return false;
    }

    std::wstring wideText;
    if (!utf8ToWide(text, wideText)) {
        return false;
    }

    Gdiplus::FontFamily family(nativeTextFont_->familyName.c_str(), &nativeTextFont_->collection);
    if (!family.IsAvailable()) {
        return false;
    }

    const float fontSize = static_cast<float>(std::max(1, scale) * 8);
    Gdiplus::Font font(&family, fontSize, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Gdiplus::Bitmap measureBitmap(1, 1, PixelFormat32bppARGB);
    Gdiplus::Graphics measureGraphics(&measureBitmap);
    measureGraphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);

    std::vector<std::wstring> lines;
    std::wstring current;
    for (wchar_t ch : wideText) {
        if (ch == L'\n') {
            lines.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    lines.push_back(current);

    const float lineHeight = std::ceil(font.GetHeight(&measureGraphics) + 2.0f);
    float maxWidth = 1.0f;
    for (const std::wstring& line : lines) {
        if (line.empty()) {
            continue;
        }
        Gdiplus::RectF bounds{};
        measureGraphics.MeasureString(line.c_str(), static_cast<INT>(line.size()), &font, Gdiplus::PointF{0.0f, 0.0f}, &bounds);
        maxWidth = std::max(maxWidth, std::ceil(bounds.Width + 4.0f));
    }

    const int bitmapWidth = std::max(1, static_cast<int>(maxWidth));
    const int bitmapHeight = std::max(1, static_cast<int>(lineHeight * static_cast<float>(lines.size()) + 4.0f));
    Gdiplus::Bitmap bitmap(bitmapWidth, bitmapHeight, PixelFormat32bppARGB);
    Gdiplus::Graphics graphics(&bitmap);
    graphics.Clear(Gdiplus::Color(0, 0, 0, 0));
    graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

    Gdiplus::SolidBrush brush(Gdiplus::Color(color.a, color.r, color.g, color.b));
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (lines[i].empty()) {
            continue;
        }
        graphics.DrawString(
            lines[i].c_str(),
            static_cast<INT>(lines[i].size()),
            &font,
            Gdiplus::PointF{0.0f, static_cast<float>(i) * lineHeight},
            &brush);
    }

    Gdiplus::Rect lockRect(0, 0, bitmapWidth, bitmapHeight);
    Gdiplus::BitmapData locked{};
    if (bitmap.LockBits(&lockRect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &locked) != Gdiplus::Ok) {
        return false;
    }

    const int rowBytes = bitmapWidth * 4;
    std::vector<unsigned char> pixels(static_cast<std::size_t>(rowBytes) * static_cast<std::size_t>(bitmapHeight));
    const auto* source = static_cast<const unsigned char*>(locked.Scan0);
    const int sourceStride = locked.Stride;
    for (int y = 0; y < bitmapHeight; ++y) {
        const int sourceY = sourceStride < 0 ? bitmapHeight - 1 - y : y;
        const unsigned char* sourceRow = source + sourceY * std::abs(sourceStride);
        unsigned char* targetRow = pixels.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(rowBytes);
        std::memcpy(targetRow, sourceRow, static_cast<std::size_t>(rowBytes));
    }
    bitmap.UnlockBits(&locked);

    SDL_Surface* surface = SDL_CreateSurfaceFrom(bitmapWidth, bitmapHeight, SDL_PIXELFORMAT_BGRA32, pixels.data(), rowBytes);
    if (!surface) {
        return false;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    SDL_DestroySurface(surface);
    if (!texture) {
        return false;
    }

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
    const SDL_FRect dst{pos.x, pos.y, static_cast<float>(bitmapWidth), static_cast<float>(bitmapHeight)};
    SDL_RenderTexture(renderer_, texture, nullptr, &dst);
    SDL_DestroyTexture(texture);
    return true;
#else
    (void)pos;
    (void)text;
    (void)color;
    (void)scale;
    return false;
#endif
}

void Renderer::unloadIconSheet()
{
    if (iconSheet_.texture) {
        SDL_DestroyTexture(iconSheet_.texture);
        iconSheet_.texture = nullptr;
    }
    iconSheet_.columns = 0;
    iconSheet_.rows = 0;
}

bool Renderer::loadIconSheet(std::string_view path, int iconSize, int columns, int rows)
{
    unloadIconSheet();
    lastAssetError_.clear();

    if (iconSize <= 0 || columns <= 0 || rows <= 0) {
        lastAssetError_ = "Invalid icon sheet layout";
        return false;
    }

    const std::string pathString(path);
    const int expectedWidth = iconSize * columns;
    const int expectedHeight = iconSize * rows;

#ifdef _WIN32
    static GdiPlusSession gdiPlus;
    if (!gdiPlus.initialized()) {
        lastAssetError_ = "GDI+ startup failed";
        return false;
    }

    std::wstring widePath;
    if (!utf8ToWide(pathString, widePath)) {
        lastAssetError_ = "Invalid UTF-8 asset path: " + pathString;
        return false;
    }

    Gdiplus::Bitmap bitmap(widePath.c_str());
    if (bitmap.GetLastStatus() != Gdiplus::Ok) {
        lastAssetError_ = "Failed to load icon sheet: " + pathString;
        return false;
    }

    const int width = static_cast<int>(bitmap.GetWidth());
    const int height = static_cast<int>(bitmap.GetHeight());
    if (width != expectedWidth || height != expectedHeight) {
        lastAssetError_ = "Icon sheet must be " + std::to_string(expectedWidth) + "x" + std::to_string(expectedHeight) +
            " pixels, got " + std::to_string(width) + "x" + std::to_string(height);
        return false;
    }

    Gdiplus::Rect rect(0, 0, width, height);
    Gdiplus::BitmapData locked{};
    if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &locked) != Gdiplus::Ok) {
        lastAssetError_ = "Failed to lock icon sheet pixels: " + pathString;
        return false;
    }

    std::vector<unsigned char> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 4U);
    const auto* source = static_cast<const unsigned char*>(locked.Scan0);
    const int sourceStride = locked.Stride;
    const int rowBytes = width * 4;
    for (int y = 0; y < height; ++y) {
        const int sourceY = sourceStride < 0 ? height - 1 - y : y;
        const unsigned char* sourceRow = source + sourceY * std::abs(sourceStride);
        unsigned char* targetRow = pixels.data() + static_cast<size_t>(y) * static_cast<size_t>(rowBytes);
        std::memcpy(targetRow, sourceRow, static_cast<size_t>(rowBytes));
    }
    bitmap.UnlockBits(&locked);

    SDL_Surface* surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_BGRA32, pixels.data(), rowBytes);
    if (!surface) {
        lastAssetError_ = std::string("SDL_CreateSurfaceFrom failed: ") + SDL_GetError();
        return false;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    SDL_DestroySurface(surface);
    if (!texture) {
        lastAssetError_ = std::string("SDL_CreateTextureFromSurface failed: ") + SDL_GetError();
        return false;
    }

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

    iconSheet_.texture = texture;
    iconSheet_.iconSize = iconSize;
    iconSheet_.columns = columns;
    iconSheet_.rows = rows;
    return true;
#else
    lastAssetError_ = "PNG icon sheet loading is only implemented on Windows";
    return false;
#endif
}

void Renderer::drawIcon(int index, Vec2 pos, float scale, Color tint)
{
    const float size = static_cast<float>(iconSheet_.iconSize) * scale;
    drawIcon(index, pos, {size, size}, tint);
}

void Renderer::drawIcon(int index, Vec2 pos, Vec2 size, Color tint)
{
    if (!iconSheet_.texture || index < 0 || index >= iconSheet_.columns * iconSheet_.rows) {
        return;
    }

    const int sourceX = (index % iconSheet_.columns) * iconSheet_.iconSize;
    const int sourceY = (index / iconSheet_.columns) * iconSheet_.iconSize;
    const SDL_FRect src{
        static_cast<float>(sourceX),
        static_cast<float>(sourceY),
        static_cast<float>(iconSheet_.iconSize),
        static_cast<float>(iconSheet_.iconSize)
    };
    const Vec2 p = transform(pos);
    const SDL_FRect dst{p.x, p.y, size.x, size.y};

    SDL_SetTextureColorMod(iconSheet_.texture, tint.r, tint.g, tint.b);
    SDL_SetTextureAlphaMod(iconSheet_.texture, tint.a);
    SDL_RenderTexture(renderer_, iconSheet_.texture, &src, &dst);
}

}
