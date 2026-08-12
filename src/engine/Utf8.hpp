#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace majo::utf8 {

struct DecodedCodepoint {
    char32_t value = U'\0';
    std::size_t byteLength = 0;
    bool valid = false;
};

[[nodiscard]] constexpr bool isContinuationByte(unsigned char byte)
{
    return (byte & 0xc0u) == 0x80u;
}

[[nodiscard]] constexpr DecodedCodepoint decodeCodepoint(std::string_view text, std::size_t offset)
{
    if (offset >= text.size()) {
        return {};
    }

    const auto byteAt = [&](std::size_t relativeOffset) {
        return static_cast<unsigned char>(text[offset + relativeOffset]);
    };
    const unsigned char first = byteAt(0);
    if (first <= 0x7fu) {
        return {static_cast<char32_t>(first), 1, true};
    }

    if (first >= 0xc2u && first <= 0xdfu) {
        if (text.size() - offset < 2 || !isContinuationByte(byteAt(1))) {
            return {U'\0', 1, false};
        }
        const char32_t value =
            (static_cast<char32_t>(first & 0x1fu) << 6) |
            static_cast<char32_t>(byteAt(1) & 0x3fu);
        return {value, 2, true};
    }

    if (first >= 0xe0u && first <= 0xefu) {
        if (text.size() - offset < 3) {
            return {U'\0', 1, false};
        }
        const unsigned char second = byteAt(1);
        const unsigned char third = byteAt(2);
        const bool secondValid = isContinuationByte(second) &&
            (first != 0xe0u || second >= 0xa0u) &&
            (first != 0xedu || second <= 0x9fu);
        if (!secondValid || !isContinuationByte(third)) {
            return {U'\0', 1, false};
        }
        const char32_t value =
            (static_cast<char32_t>(first & 0x0fu) << 12) |
            (static_cast<char32_t>(second & 0x3fu) << 6) |
            static_cast<char32_t>(third & 0x3fu);
        return {value, 3, true};
    }

    if (first >= 0xf0u && first <= 0xf4u) {
        if (text.size() - offset < 4) {
            return {U'\0', 1, false};
        }
        const unsigned char second = byteAt(1);
        const unsigned char third = byteAt(2);
        const unsigned char fourth = byteAt(3);
        const bool secondValid = isContinuationByte(second) &&
            (first != 0xf0u || second >= 0x90u) &&
            (first != 0xf4u || second <= 0x8fu);
        if (!secondValid || !isContinuationByte(third) || !isContinuationByte(fourth)) {
            return {U'\0', 1, false};
        }
        const char32_t value =
            (static_cast<char32_t>(first & 0x07u) << 18) |
            (static_cast<char32_t>(second & 0x3fu) << 12) |
            (static_cast<char32_t>(third & 0x3fu) << 6) |
            static_cast<char32_t>(fourth & 0x3fu);
        return {value, 4, true};
    }

    return {U'\0', 1, false};
}

[[nodiscard]] constexpr std::size_t codepointCount(std::string_view text)
{
    std::size_t count = 0;
    for (std::size_t offset = 0; offset < text.size();) {
        const DecodedCodepoint decoded = decodeCodepoint(text, offset);
        if (decoded.valid) {
            ++count;
        }
        offset += decoded.byteLength;
    }
    return count;
}

[[nodiscard]] inline std::string sanitized(std::string_view text)
{
    std::string result;
    result.reserve(text.size());
    for (std::size_t offset = 0; offset < text.size();) {
        const DecodedCodepoint decoded = decodeCodepoint(text, offset);
        if (decoded.valid) {
            result.append(text.substr(offset, decoded.byteLength));
        }
        offset += decoded.byteLength;
    }
    return result;
}

inline bool sanitizeInPlace(std::string& text)
{
    std::string validText = sanitized(text);
    if (validText == text) {
        return false;
    }
    text = std::move(validText);
    return true;
}

inline bool eraseLastCodepoint(std::string& text)
{
    const bool repaired = sanitizeInPlace(text);
    if (repaired || text.empty()) {
        return repaired;
    }

    std::size_t start = text.size() - 1;
    while (start > 0 && isContinuationByte(static_cast<unsigned char>(text[start]))) {
        --start;
    }
    text.erase(start);
    return true;
}

static_assert(decodeCodepoint("A", 0).valid && decodeCodepoint("A", 0).byteLength == 1);
static_assert(decodeCodepoint("\xe3\x81\x82", 0).valid && decodeCodepoint("\xe3\x81\x82", 0).byteLength == 3);
static_assert(decodeCodepoint("\xf0\x9f\x98\x80", 0).valid && decodeCodepoint("\xf0\x9f\x98\x80", 0).byteLength == 4);
static_assert(!decodeCodepoint("\xe3", 0).valid);
static_assert(!decodeCodepoint("\xc0\x80", 0).valid);
static_assert(!decodeCodepoint("\xed\xa0\x80", 0).valid);
static_assert(!decodeCodepoint("\xf4\x90\x80\x80", 0).valid);

}
