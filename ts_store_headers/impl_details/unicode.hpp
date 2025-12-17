//
// impl_details/unicode.hpp — Unicode helpers for payloads
//

#pragma once

// Thread-specific Unicode suffix (visible emoji range)
inline char32_t get_thread_unicode_suffix(int thread_id) noexcept {
    constexpr char32_t base = 0x1F600;  // 😀 grinning face
    return base + (thread_id % 80);
}

// Encode single codepoint to UTF-8 (returns bytes written)
inline static size_t encode_utf8(char* dst, char32_t cp) noexcept {
    if (cp <= 0x7F) {
        dst[0] = static_cast<char>(cp);
        return 1;
    }
    if (cp <= 0x7FF) {
        dst[0] = static_cast<char>(0xC0 | (cp >> 6));
        dst[1] = static_cast<char>(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp <= 0xFFFF) {
        dst[0] = static_cast<char>(0xE0 | (cp >> 12));
        dst[1] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        dst[2] = static_cast<char>(0x80 | (cp & 0x3F));
        return 3;
    }
    // 4-byte for emojis
    dst[0] = static_cast<char>(0xF0 | (cp >> 18));
    dst[1] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    dst[2] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    dst[3] = static_cast<char>(0x80 | (cp & 0x3F));
    return 4;
}

private:
static char* itoa(char* dst, int value) noexcept {
    if (value == 0) {
        *dst++ = '0';
        return dst;
    }

    char tmp[12];
    char* t = tmp + sizeof(tmp);
    bool neg = value < 0;
    unsigned int v = neg ? -static_cast<unsigned int>(value) : static_cast<unsigned int>(value);

    do {
        *--t = '0' + (v % 10);
        v /= 10;
    } while (v);

    if (neg) *--t = '-';

    std::memcpy(dst, t, static_cast<std::size_t>(tmp + sizeof(tmp) - t));
    return dst + (tmp + sizeof(tmp) - t);
}

