#include "engine/Renderer.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <sstream>
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

bool isUiGuidePixel(unsigned char b, unsigned char g, unsigned char r, unsigned char a)
{
    return a <= 8 || (r <= 8 && g <= 8 && b <= 8);
}

std::vector<int> collapseGuideRuns(const std::vector<int>& guides, int limit)
{
    std::vector<int> collapsed;
    for (std::size_t i = 0; i < guides.size();) {
        const int start = guides[i];
        int end = start;
        ++i;
        while (i < guides.size() && guides[i] == end + 1) {
            end = guides[i];
            ++i;
        }
        if (start == 0 || end >= limit - 1) {
            continue;
        }
        collapsed.push_back((start + end) / 2);
    }
    return collapsed;
}

std::size_t utf8CodepointLength(unsigned char lead)
{
    if ((lead & 0x80U) == 0) {
        return 1;
    }
    if ((lead & 0xe0U) == 0xc0U) {
        return 2;
    }
    if ((lead & 0xf0U) == 0xe0U) {
        return 3;
    }
    if ((lead & 0xf8U) == 0xf0U) {
        return 4;
    }
    return 1;
}

std::string textCacheKey(std::string_view text, Color color, int scale)
{
    std::ostringstream out;
    out << scale << ':'
        << static_cast<int>(color.r) << ','
        << static_cast<int>(color.g) << ','
        << static_cast<int>(color.b) << ','
        << static_cast<int>(color.a) << ':'
        << text;
    return out.str();
}

std::string textMeasureCacheKey(std::string_view text, int scale)
{
    std::ostringstream out;
    out << scale << ':' << text;
    return out.str();
}

std::string wrappedTextCacheKey(std::string_view text, float maxWidth, int scale)
{
    std::ostringstream out;
    out << scale << ':' << static_cast<int>(std::lround(maxWidth * 10.0f)) << ':' << text;
    return out.str();
}

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
    clearTextCache();
    unloadSpriteSheet(iconSheet_);
    unloadSpriteSheet(playerSheet_);
    unloadUiWindowTexture();
    unloadUiSubWindowTexture();
    unloadUiButtonTexture();
}

void Renderer::setColor(Color color)
{
    color = transformColor(color);
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
}

Vec2 Renderer::transform(Vec2 p) const
{
    p = camera_ ? camera_->worldToScreen(p) : p;
    for (const ScreenTransform& transform : screenTransforms_) {
        p = transform.origin + (p - transform.origin) * transform.scale;
    }
    return p;
}

Vec2 Renderer::transformSize(Vec2 size) const
{
    const float scale = screenScale();
    return {size.x * scale, size.y * scale};
}

Color Renderer::transformColor(Color color) const
{
    float alpha = static_cast<float>(color.a);
    for (const ScreenTransform& transform : screenTransforms_) {
        alpha *= std::clamp(transform.alpha, 0.0f, 1.0f);
    }
    color.a = static_cast<unsigned char>(std::clamp(std::lround(alpha), 0L, 255L));
    return color;
}

float Renderer::screenScale() const
{
    float scale = 1.0f;
    for (const ScreenTransform& transform : screenTransforms_) {
        scale *= transform.scale;
    }
    return scale;
}

void Renderer::pushScreenTransform(Vec2 origin, float scale, float alpha)
{
    screenTransforms_.push_back({origin, scale, alpha});
}

void Renderer::popScreenTransform()
{
    if (!screenTransforms_.empty()) {
        screenTransforms_.pop_back();
    }
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
    const Vec2 s = transformSize(size);
    SDL_FRect rect{p.x, p.y, s.x, s.y};
    setColor(color);
    SDL_RenderFillRect(renderer_, &rect);
}

void Renderer::drawRect(Vec2 pos, Vec2 size, Color color)
{
    const Vec2 p = transform(pos);
    const Vec2 s = transformSize(size);
    SDL_FRect rect{p.x, p.y, s.x, s.y};
    setColor(color);
    SDL_RenderRect(renderer_, &rect);
}

void Renderer::fillCircle(Vec2 center, float radius, Color color)
{
    const Vec2 c = transform(center);
    radius *= screenScale();
    setColor(color);
    for (int y = static_cast<int>(-radius); y <= static_cast<int>(radius); ++y) {
        const float span = std::sqrt(std::max(0.0f, radius * radius - static_cast<float>(y * y)));
        SDL_RenderLine(renderer_, c.x - span, c.y + y, c.x + span, c.y + y);
    }
}

void Renderer::drawCircle(Vec2 center, float radius, Color color)
{
    const Vec2 c = transform(center);
    radius *= screenScale();
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
                const Vec2 p = transform(pos + Vec2{x * static_cast<float>(scale), y * static_cast<float>(scale)});
                const Vec2 s = transformSize({static_cast<float>(scale), static_cast<float>(scale)});
                SDL_FRect rect{p.x, p.y, s.x, s.y};
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

Vec2 Renderer::measureText(std::string_view text, int scale)
{
    const std::string key = textMeasureCacheKey(text, std::max(1, scale));
    if (const auto it = textMeasureCache_.find(key); it != textMeasureCache_.end()) {
        return it->second;
    }

    Vec2 measured{};
#ifdef _WIN32
    if (measureNativeText(text, scale, measured)) {
        if (textMeasureCache_.size() > 2048) {
            textMeasureCache_.clear();
        }
        textMeasureCache_[key] = measured;
        return measured;
    }
#endif

    float width = 0.0f;
    float lineWidth = 0.0f;
    float height = 9.0f * static_cast<float>(std::max(1, scale));
    for (unsigned char c : text) {
        if (c == '\n') {
            width = std::max(width, lineWidth);
            lineWidth = 0.0f;
            height += 9.0f * static_cast<float>(std::max(1, scale));
            continue;
        }
        lineWidth += 6.0f * static_cast<float>(std::max(1, scale));
    }
    measured = {std::max(width, lineWidth), height};
    if (textMeasureCache_.size() > 2048) {
        textMeasureCache_.clear();
    }
    textMeasureCache_[key] = measured;
    return measured;
}

void Renderer::drawWrappedText(Vec2 pos, std::string_view text, float maxWidth, Color color, int scale)
{
    drawText(pos, wrappedText(text, maxWidth, scale), color, scale);
}

Vec2 Renderer::measureWrappedText(std::string_view text, float maxWidth, int scale)
{
    return measureText(wrappedText(text, maxWidth, scale), scale);
}

bool Renderer::loadTextFont(std::string_view path)
{
    lastAssetError_.clear();
    clearTextCache();
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

    const Color drawColor = transformColor(color);
    Color cacheColor = color;
    cacheColor.a = 255;
    const std::string key = textCacheKey(text, cacheColor, std::max(1, scale));
    auto it = textCache_.find(key);
    if (it == textCache_.end()) {
        if (textCache_.size() > 2048) {
            clearTextCache();
        }
        TextTexture texture{};
        if (!renderNativeTextToTexture(text, cacheColor, scale, texture)) {
            return false;
        }
        it = textCache_.emplace(key, texture).first;
    }
    if (!it->second.texture) {
        return false;
    }

    const Vec2 p = transform(pos);
    const Vec2 s = transformSize({static_cast<float>(it->second.width), static_cast<float>(it->second.height)});
    const SDL_FRect dst{p.x, p.y, s.x, s.y};
    SDL_SetTextureAlphaMod(it->second.texture, drawColor.a);
    SDL_RenderTexture(renderer_, it->second.texture, nullptr, &dst);
    return true;
#else
    (void)pos;
    (void)text;
    (void)color;
    (void)scale;
    return false;
#endif
}

bool Renderer::measureNativeText(std::string_view text, int scale, Vec2& outSize)
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

    outSize = {maxWidth, std::ceil(lineHeight * static_cast<float>(lines.size()) + 4.0f)};
    return true;
#else
    (void)text;
    (void)scale;
    (void)outSize;
    return false;
#endif
}

bool Renderer::renderNativeTextToTexture(std::string_view text, Color color, int scale, TextTexture& outTexture)
{
#ifdef _WIN32
    std::wstring wideText;
    if (!utf8ToWide(text, wideText)) {
        return false;
    }

    Gdiplus::FontFamily family(nativeTextFont_->familyName.c_str(), &nativeTextFont_->collection);
    if (!family.IsAvailable()) {
        return false;
    }

    Vec2 measured{};
    if (!measureNativeText(text, scale, measured)) {
        return false;
    }

    const int bitmapWidth = std::max(1, static_cast<int>(std::ceil(measured.x)));
    const int bitmapHeight = std::max(1, static_cast<int>(std::ceil(measured.y)));
    const float fontSize = static_cast<float>(std::max(1, scale) * 8);
    Gdiplus::Font font(&family, fontSize, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Gdiplus::Bitmap measureBitmap(1, 1, PixelFormat32bppARGB);
    Gdiplus::Graphics measureGraphics(&measureBitmap);
    const float lineHeight = std::ceil(font.GetHeight(&measureGraphics) + 2.0f);

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
    outTexture.texture = texture;
    outTexture.width = bitmapWidth;
    outTexture.height = bitmapHeight;
    return true;
#else
    (void)text;
    (void)color;
    (void)scale;
    (void)outTexture;
    return false;
#endif
}

void Renderer::clearTextCache()
{
    for (auto& [_, cached] : textCache_) {
        if (cached.texture) {
            SDL_DestroyTexture(cached.texture);
            cached.texture = nullptr;
        }
    }
    textCache_.clear();
    textMeasureCache_.clear();
    wrappedTextCache_.clear();
}

std::string Renderer::wrappedText(std::string_view text, float maxWidth, int scale)
{
    if (maxWidth <= 0.0f || text.empty()) {
        return std::string(text);
    }

    const std::string key = wrappedTextCacheKey(text, maxWidth, std::max(1, scale));
    if (const auto it = wrappedTextCache_.find(key); it != wrappedTextCache_.end()) {
        return it->second;
    }

    std::string output;
    std::string line;
    for (std::size_t i = 0; i < text.size();) {
        const char c = text[i];
        if (c == '\n') {
            output += line;
            output.push_back('\n');
            line.clear();
            ++i;
            continue;
        }

        const std::size_t charLength = std::min(utf8CodepointLength(static_cast<unsigned char>(c)), text.size() - i);
        const std::string_view token{text.data() + i, charLength};
        std::string candidate = line;
        candidate.append(token);
        if (!line.empty() && measureText(candidate, scale).x > maxWidth) {
            output += line;
            output.push_back('\n');
            line.assign(token);
        } else {
            line = std::move(candidate);
        }
        i += charLength;
    }

    output += line;
    if (wrappedTextCache_.size() > 1024) {
        wrappedTextCache_.clear();
    }
    wrappedTextCache_[key] = output;
    return output;
}

void Renderer::unloadIconSheet()
{
    unloadSpriteSheet(iconSheet_);
}

bool Renderer::loadIconSheet(std::string_view path, int iconSize, int columns, int rows)
{
    return loadSpriteSheet(path, iconSize, columns, rows, "icon sheet", iconSheet_);
}

void Renderer::unloadPlayerSheet()
{
    unloadSpriteSheet(playerSheet_);
}

bool Renderer::loadPlayerSheet(std::string_view path, int frameSize, int columns, int rows)
{
    return loadSpriteSheet(path, frameSize, columns, rows, "player sheet", playerSheet_);
}

void Renderer::unloadUiWindowTexture()
{
    unloadGuidedTexture(uiWindowTexture_);
}

void Renderer::unloadUiSubWindowTexture()
{
    unloadGuidedTexture(uiSubWindowTexture_);
}

void Renderer::unloadUiButtonTexture()
{
    unloadGuidedTexture(uiButtonTexture_);
}

void Renderer::unloadSpriteSheet(SpriteSheet& sheet)
{
    if (sheet.texture) {
        SDL_DestroyTexture(sheet.texture);
        sheet.texture = nullptr;
    }
    sheet.columns = 0;
    sheet.rows = 0;
}

bool Renderer::loadSpriteSheet(std::string_view path, int frameSize, int columns, int rows, std::string_view label, SpriteSheet& sheet)
{
    lastAssetError_.clear();

    const std::string labelString(label);
    if (frameSize <= 0 || columns <= 0 || rows <= 0) {
        lastAssetError_ = "Invalid " + labelString + " layout";
        return false;
    }

    const std::string pathString(path);
    const int expectedWidth = frameSize * columns;
    const int expectedHeight = frameSize * rows;

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
        lastAssetError_ = "Failed to load " + labelString + ": " + pathString;
        return false;
    }

    const int width = static_cast<int>(bitmap.GetWidth());
    const int height = static_cast<int>(bitmap.GetHeight());
    if (width != expectedWidth || height != expectedHeight) {
        lastAssetError_ = labelString + " must be " + std::to_string(expectedWidth) + "x" + std::to_string(expectedHeight) +
            " pixels, got " + std::to_string(width) + "x" + std::to_string(height);
        return false;
    }

    Gdiplus::Rect rect(0, 0, width, height);
    Gdiplus::BitmapData locked{};
    if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &locked) != Gdiplus::Ok) {
        lastAssetError_ = "Failed to lock " + labelString + " pixels: " + pathString;
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

    unloadSpriteSheet(sheet);
    sheet.texture = texture;
    sheet.frameSize = frameSize;
    sheet.columns = columns;
    sheet.rows = rows;
    return true;
#else
    lastAssetError_ = "PNG sprite sheet loading is only implemented on Windows";
    return false;
#endif
}

void Renderer::unloadGuidedTexture(GuidedTexture& texture)
{
    if (texture.texture) {
        SDL_DestroyTexture(texture.texture);
    }
    texture = {};
}

bool Renderer::loadUiWindowTexture(std::string_view path)
{
    return loadGuidedTexture(path, 5, 3, false, "UI window texture", uiWindowTexture_);
}

bool Renderer::loadUiSubWindowTexture(std::string_view path)
{
    return loadGuidedTexture(path, 3, 3, true, "UI sub-window texture", uiSubWindowTexture_);
}

bool Renderer::loadUiButtonTexture(std::string_view path)
{
    return loadGuidedTexture(path, 3, 3, true, "UI button texture", uiButtonTexture_);
}

bool Renderer::loadGuidedTexture(std::string_view path, int columns, int rows, bool transparentOnly, std::string_view label, GuidedTexture& target)
{
    lastAssetError_.clear();
    const std::string pathString(path);
    const std::string labelString(label);

#ifdef _WIN32
    if (columns <= 0 || columns > 5 || rows <= 0 || rows > 3) {
        lastAssetError_ = "Invalid " + labelString + " layout";
        return false;
    }

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
        lastAssetError_ = "Failed to load " + labelString + ": " + pathString;
        return false;
    }

    const int width = static_cast<int>(bitmap.GetWidth());
    const int height = static_cast<int>(bitmap.GetHeight());
    if (width <= 0 || height <= 0) {
        lastAssetError_ = "Invalid " + labelString + " size: " + pathString;
        return false;
    }

    Gdiplus::Rect rect(0, 0, width, height);
    Gdiplus::BitmapData locked{};
    if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &locked) != Gdiplus::Ok) {
        lastAssetError_ = "Failed to lock " + labelString + " pixels: " + pathString;
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

    auto isGuide = [&](const unsigned char* pixel) {
        return transparentOnly
            ? pixel[3] <= 8
            : isUiGuidePixel(pixel[0], pixel[1], pixel[2], pixel[3]);
    };

    std::vector<int> verticalGuides;
    for (int x = 0; x < width; ++x) {
        int guidePixels = 0;
        for (int y = 0; y < height; ++y) {
            const auto* pixel = pixels.data() + (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4U;
            if (isGuide(pixel)) {
                ++guidePixels;
            }
        }
        if (guidePixels >= static_cast<int>(height * 0.95f)) {
            verticalGuides.push_back(x);
        }
    }

    std::vector<int> horizontalGuides;
    for (int y = 0; y < height; ++y) {
        int guidePixels = 0;
        for (int x = 0; x < width; ++x) {
            const auto* pixel = pixels.data() + (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4U;
            if (isGuide(pixel)) {
                ++guidePixels;
            }
        }
        if (guidePixels >= static_cast<int>(width * 0.95f)) {
            horizontalGuides.push_back(y);
        }
    }

    verticalGuides = collapseGuideRuns(verticalGuides, width);
    horizontalGuides = collapseGuideRuns(horizontalGuides, height);
    if (verticalGuides.size() != static_cast<std::size_t>(columns - 1) ||
        horizontalGuides.size() != static_cast<std::size_t>(rows - 1)) {
        lastAssetError_ = labelString + " must contain " + std::to_string(columns - 1) +
            " vertical and " + std::to_string(rows - 1) + " horizontal guide lines: " + pathString;
        return false;
    }

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

    GuidedTexture loaded;
    loaded.texture = texture;
    loaded.width = width;
    loaded.height = height;
    loaded.valid = true;
    loaded.columns = columns;
    loaded.rows = rows;

    std::array<int, 6> xEdges{};
    std::array<int, 5> xSeparators{};
    xEdges[0] = 0;
    for (int i = 0; i < columns - 1; ++i) {
        xSeparators[static_cast<std::size_t>(i)] = verticalGuides[static_cast<std::size_t>(i)];
        xEdges[static_cast<std::size_t>(i + 1)] = verticalGuides[static_cast<std::size_t>(i)] + 1;
    }
    xEdges[static_cast<std::size_t>(columns)] = width;

    std::array<int, 4> yEdges{};
    std::array<int, 3> ySeparators{};
    yEdges[0] = 0;
    for (int i = 0; i < rows - 1; ++i) {
        ySeparators[static_cast<std::size_t>(i)] = horizontalGuides[static_cast<std::size_t>(i)];
        yEdges[static_cast<std::size_t>(i + 1)] = horizontalGuides[static_cast<std::size_t>(i)] + 1;
    }
    yEdges[static_cast<std::size_t>(rows)] = height;

    for (int col = 0; col < columns; ++col) {
        const int start = xEdges[static_cast<std::size_t>(col)];
        const int end = col < columns - 1 ? xSeparators[static_cast<std::size_t>(col)] : width;
        loaded.columnWidths[static_cast<std::size_t>(col)] = static_cast<float>(end - start);
    }
    for (int row = 0; row < rows; ++row) {
        const int start = yEdges[static_cast<std::size_t>(row)];
        const int end = row < rows - 1 ? ySeparators[static_cast<std::size_t>(row)] : height;
        loaded.rowHeights[static_cast<std::size_t>(row)] = static_cast<float>(end - start);
    }
    for (int row = 0; row < rows; ++row) {
        const int y = yEdges[static_cast<std::size_t>(row)];
        const int bottom = row < rows - 1 ? ySeparators[static_cast<std::size_t>(row)] : height;
        for (int col = 0; col < columns; ++col) {
            const int x = xEdges[static_cast<std::size_t>(col)];
            const int right = col < columns - 1 ? xSeparators[static_cast<std::size_t>(col)] : width;
            loaded.cells[static_cast<std::size_t>(row * columns + col)] = {
                static_cast<float>(x),
                static_cast<float>(y),
                static_cast<float>(right - x),
                static_cast<float>(bottom - y),
            };
        }
    }
    unloadGuidedTexture(target);
    target = loaded;
    return true;
#else
    lastAssetError_ = "PNG guided texture loading is only implemented on Windows";
    return false;
#endif
}

Vec2 Renderer::uiWindowMinSize() const
{
    if (!hasUiWindowTexture()) {
        return {};
    }
    float width = 0.0f;
    for (float column : uiWindowTexture_.columnWidths) {
        width += column;
    }
    float height = 0.0f;
    for (float row : uiWindowTexture_.rowHeights) {
        height += row;
    }
    return {width, height};
}

void Renderer::drawNineSliceFrame(const GuidedTexture& texture, Vec2 pos, Vec2 size, Color tint)
{
    if (!texture.texture || !texture.valid || texture.columns != 3 || texture.rows != 3) {
        return;
    }

    const auto& cw = texture.columnWidths;
    const auto& rh = texture.rowHeights;
    const float fixedWidth = cw[0] + cw[2];
    const float fixedHeight = rh[0] + rh[2];
    const float scaleX = fixedWidth > 0.0f ? std::min(1.0f, size.x / fixedWidth) : 1.0f;
    const float scaleY = fixedHeight > 0.0f ? std::min(1.0f, size.y / fixedHeight) : 1.0f;
    const float leftWidth = cw[0] * scaleX;
    const float rightWidth = cw[2] * scaleX;
    const float topHeight = rh[0] * scaleY;
    const float bottomHeight = rh[2] * scaleY;
    const float centerWidth = std::max(0.0f, size.x - leftWidth - rightWidth);
    const float centerHeight = std::max(0.0f, size.y - topHeight - bottomHeight);
    const float rightX = pos.x + size.x - rightWidth;
    const float bottomY = pos.y + size.y - bottomHeight;

    auto cell = [&](int row, int col) -> SDL_FRect {
        return texture.cells[static_cast<std::size_t>(row * 3 + col)];
    };

    drawTextureRegion(texture.texture, cell(0, 0), pos, {leftWidth, topHeight}, tint);
    drawTextureTiled(texture.texture, cell(0, 1), {pos.x + leftWidth, pos.y}, {centerWidth, topHeight}, tint);
    drawTextureRegion(texture.texture, cell(0, 2), {rightX, pos.y}, {rightWidth, topHeight}, tint);

    drawTextureTiled(texture.texture, cell(1, 0), {pos.x, pos.y + topHeight}, {leftWidth, centerHeight}, tint);
    drawTextureTiled(texture.texture, cell(1, 1), {pos.x + leftWidth, pos.y + topHeight}, {centerWidth, centerHeight}, tint);
    drawTextureTiled(texture.texture, cell(1, 2), {rightX, pos.y + topHeight}, {rightWidth, centerHeight}, tint);

    drawTextureRegion(texture.texture, cell(2, 0), {pos.x, bottomY}, {leftWidth, bottomHeight}, tint);
    drawTextureTiled(texture.texture, cell(2, 1), {pos.x + leftWidth, bottomY}, {centerWidth, bottomHeight}, tint);
    drawTextureRegion(texture.texture, cell(2, 2), {rightX, bottomY}, {rightWidth, bottomHeight}, tint);
}

void Renderer::drawHorizontalSliceRow(const GuidedTexture& texture, int row, Vec2 pos, float width, Color tint)
{
    if (!texture.texture || !texture.valid || texture.columns != 3 || row < 0 || row >= texture.rows) {
        return;
    }

    auto cell = [&](int col) -> SDL_FRect {
        return texture.cells[static_cast<std::size_t>(row * texture.columns + col)];
    };

    const float leftWidth = texture.columnWidths[0];
    const float rightWidth = texture.columnWidths[2];
    const float height = texture.rowHeights[static_cast<std::size_t>(row)];
    const float fixedWidth = leftWidth + rightWidth;
    const float scaleX = fixedWidth > 0.0f && width < fixedWidth ? width / fixedWidth : 1.0f;
    const float dstLeftWidth = leftWidth * scaleX;
    const float dstRightWidth = rightWidth * scaleX;
    const float dstCenterWidth = std::max(0.0f, width - dstLeftWidth - dstRightWidth);

    drawTextureRegion(texture.texture, cell(0), pos, {dstLeftWidth, height}, tint);
    drawTextureTiled(texture.texture, cell(1), {pos.x + dstLeftWidth, pos.y}, {dstCenterWidth, height}, tint);
    drawTextureRegion(texture.texture, cell(2), {pos.x + width - dstRightWidth, pos.y}, {dstRightWidth, height}, tint);
}

void Renderer::drawIcon(int index, Vec2 pos, float scale, Color tint)
{
    const float size = static_cast<float>(iconSheet_.frameSize) * scale;
    drawIcon(index, pos, {size, size}, tint);
}

void Renderer::drawIcon(int index, Vec2 pos, Vec2 size, Color tint)
{
    if (!iconSheet_.texture || index < 0 || index >= iconSheet_.columns * iconSheet_.rows) {
        return;
    }

    const int sourceX = (index % iconSheet_.columns) * iconSheet_.frameSize;
    const int sourceY = (index / iconSheet_.columns) * iconSheet_.frameSize;
    const SDL_FRect src{
        static_cast<float>(sourceX),
        static_cast<float>(sourceY),
        static_cast<float>(iconSheet_.frameSize),
        static_cast<float>(iconSheet_.frameSize)
    };
    const Vec2 p = transform(pos);
    const Vec2 s = transformSize(size);
    const SDL_FRect dst{p.x, p.y, s.x, s.y};

    tint = transformColor(tint);
    SDL_SetTextureColorMod(iconSheet_.texture, tint.r, tint.g, tint.b);
    SDL_SetTextureAlphaMod(iconSheet_.texture, tint.a);
    SDL_RenderTexture(renderer_, iconSheet_.texture, &src, &dst);
}

void Renderer::drawPlayerSprite(int index, Vec2 center, float size, bool flipHorizontal, Color tint)
{
    if (!playerSheet_.texture || index < 0 || index >= playerSheet_.columns * playerSheet_.rows) {
        return;
    }

    const int sourceX = (index % playerSheet_.columns) * playerSheet_.frameSize;
    const int sourceY = (index / playerSheet_.columns) * playerSheet_.frameSize;
    const SDL_FRect src{
        static_cast<float>(sourceX),
        static_cast<float>(sourceY),
        static_cast<float>(playerSheet_.frameSize),
        static_cast<float>(playerSheet_.frameSize)
    };
    const Vec2 p = transform(center - Vec2{size * 0.5f, size * 0.5f});
    const Vec2 s = transformSize({size, size});
    const SDL_FRect dst{p.x, p.y, s.x, s.y};

    tint = transformColor(tint);
    SDL_SetTextureColorMod(playerSheet_.texture, tint.r, tint.g, tint.b);
    SDL_SetTextureAlphaMod(playerSheet_.texture, tint.a);
    SDL_RenderTextureRotated(
        renderer_,
        playerSheet_.texture,
        &src,
        &dst,
        0.0,
        nullptr,
        flipHorizontal ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}

void Renderer::drawTextureRegion(SDL_Texture* texture, SDL_FRect src, Vec2 pos, Vec2 size, Color tint)
{
    if (!texture || src.w <= 0.0f || src.h <= 0.0f || size.x <= 0.0f || size.y <= 0.0f) {
        return;
    }

    const Vec2 p = transform(pos);
    const Vec2 s = transformSize(size);
    const SDL_FRect dst{p.x, p.y, s.x, s.y};
    tint = transformColor(tint);
    SDL_SetTextureColorMod(texture, tint.r, tint.g, tint.b);
    SDL_SetTextureAlphaMod(texture, tint.a);
    SDL_RenderTexture(renderer_, texture, &src, &dst);
}

void Renderer::drawTextureTiled(SDL_Texture* texture, SDL_FRect src, Vec2 pos, Vec2 size, Color tint)
{
    if (!texture || src.w <= 0.0f || src.h <= 0.0f || size.x <= 0.0f || size.y <= 0.0f) {
        return;
    }

    const bool shrinkX = size.x < src.w;
    const bool shrinkY = size.y < src.h;
    for (float y = 0.0f; y < size.y;) {
        const float dstHeight = shrinkY ? size.y : std::min(src.h, size.y - y);
        const float srcHeight = shrinkY ? src.h : dstHeight;
        for (float x = 0.0f; x < size.x;) {
            const float dstWidth = shrinkX ? size.x : std::min(src.w, size.x - x);
            const float srcWidth = shrinkX ? src.w : dstWidth;
            SDL_FRect clippedSrc{src.x, src.y, srcWidth, srcHeight};
            drawTextureRegion(texture, clippedSrc, pos + Vec2{x, y}, {dstWidth, dstHeight}, tint);
            x += dstWidth;
        }
        y += dstHeight;
    }
}

void Renderer::drawUiWindowFrame(Vec2 pos, Vec2 size, Color tint)
{
    if (!hasUiWindowTexture()) {
        return;
    }

    const auto& window = uiWindowTexture_;
    const Vec2 minSize = uiWindowMinSize();
    const float scaleX = minSize.x > 0.0f ? std::min(1.0f, size.x / minSize.x) : 1.0f;
    const float scaleY = minSize.y > 0.0f ? std::min(1.0f, size.y / minSize.y) : 1.0f;
    std::array<float, 5> cw{};
    std::array<float, 3> rh{};
    for (std::size_t i = 0; i < cw.size(); ++i) {
        cw[i] = window.columnWidths[i] * scaleX;
    }
    for (std::size_t i = 0; i < rh.size(); ++i) {
        rh[i] = window.rowHeights[i] * scaleY;
    }
    const float topHeight = rh[0];
    const float middleHeight = size.y - rh[0] - rh[2];
    const float bottomY = pos.y + topHeight + middleHeight;
    float scaledMinWidth = 0.0f;
    for (float column : cw) {
        scaledMinWidth += column;
    }
    const float extraWidth = std::max(0.0f, size.x - scaledMinWidth);

    auto cell = [&](int row, int col) -> SDL_FRect {
        return window.cells[static_cast<std::size_t>(row * 5 + col)];
    };
    auto splitExtra = [](float extra, float a, float b) -> Vec2 {
        const float total = std::max(1.0f, a + b);
        return {a + extra * (a / total), b + extra * (b / total)};
    };

    drawTextureTiled(window.texture, cell(1, 2), pos + Vec2{cw[0], topHeight}, {size.x - cw[0] - cw[4], middleHeight}, tint);

    const Vec2 topVariable = splitExtra(extraWidth, cw[1], cw[3]);
    float x = pos.x;
    drawTextureRegion(window.texture, cell(0, 0), {x, pos.y}, {cw[0], topHeight}, tint);
    x += cw[0];
    drawTextureTiled(window.texture, cell(0, 1), {x, pos.y}, {topVariable.x, topHeight}, tint);
    x += topVariable.x;
    drawTextureRegion(window.texture, cell(0, 2), {x, pos.y}, {cw[2], topHeight}, tint);
    x += cw[2];
    drawTextureTiled(window.texture, cell(0, 3), {x, pos.y}, {topVariable.y, topHeight}, tint);
    drawTextureRegion(window.texture, cell(0, 4), {pos.x + size.x - cw[4], pos.y}, {cw[4], topHeight}, tint);

    drawTextureTiled(window.texture, cell(1, 0), {pos.x, pos.y + topHeight}, {cw[0], middleHeight}, tint);
    drawTextureTiled(window.texture, cell(1, 4), {pos.x + size.x - cw[4], pos.y + topHeight}, {cw[4], middleHeight}, tint);

    const Vec2 bottomVariable = splitExtra(extraWidth, cw[1], cw[2]);
    x = pos.x;
    drawTextureRegion(window.texture, cell(2, 0), {x, bottomY}, {cw[0], rh[2]}, tint);
    x += cw[0];
    drawTextureTiled(window.texture, cell(2, 1), {x, bottomY}, {bottomVariable.x, rh[2]}, tint);
    x += bottomVariable.x;
    drawTextureTiled(window.texture, cell(2, 2), {x, bottomY}, {bottomVariable.y, rh[2]}, tint);
    x += bottomVariable.y;
    drawTextureRegion(window.texture, cell(2, 3), {x, bottomY}, {cw[3], rh[2]}, tint);
    drawTextureRegion(window.texture, cell(2, 4), {pos.x + size.x - cw[4], bottomY}, {cw[4], rh[2]}, tint);
}

void Renderer::drawUiSubWindowFrame(Vec2 pos, Vec2 size, Color tint)
{
    drawNineSliceFrame(uiSubWindowTexture_, pos, size, tint);
}

void Renderer::drawUiButtonFrame(Vec2 pos, float width, int variant, Color tint)
{
    if (!hasUiButtonTexture()) {
        return;
    }
    drawHorizontalSliceRow(uiButtonTexture_, std::clamp(variant, 0, 2), pos, width, tint);
}

}
