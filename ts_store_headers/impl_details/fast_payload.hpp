// ts_store/ts_store_headers/impl_details/fast_payload.hpp
// This file is 100% compatible with the new runtime design — NO CHANGES NEEDED!

#pragma once

#include <cstddef>
#include <string_view>
#include <cstring>

namespace jac::ts_store::inline_v001
{

template <std::size_t PayloadSize>
struct FastPayload {
    static_assert(PayloadSize >= 64, "PayloadSize too small for FastPayload");

    thread_local inline static char buffer[PayloadSize]{};
    thread_local inline static char* pos = buffer;

    static std::string_view make_fixed_payload() noexcept {
        pos = buffer;

        constexpr const char prefix[] = "Fixed Payload";
        std::memcpy(pos, prefix, sizeof(prefix) - 1);
        pos += sizeof(prefix) - 1;

        pos = itoa(pos, tid);
        *pos++ = '-';
        pos = itoa(pos, index);
        *pos++ = '\0';

        return {buffer, static_cast<std::size_t>(pos - buffer - 1)};
    }

    inline std::string_view make_fast_payload_unicode(int thread_id, int event_index) noexcept {
        thread_local static char tl_buf[BufferSize + 8]{};  // extra for UTF-8 emojis
        char* pos = tl_buf;

        constexpr const char prefix[] = "Fast-Uni: T=";
        std::memcpy(pos, prefix, sizeof(prefix)-1);
        pos += sizeof(prefix)-1;

        pos = itoa(pos, thread_id);  // reuse your existing itoa if available, or add the small one below
        *pos++ = ' ';
        pos = itoa(pos, event_index);
        *pos++ = ' ';

        // Middle padding
        std::memset(pos, '.', BufferSize - 100);  // leave room for suffix
        pos += BufferSize - 100;

        // Thread-specific Unicode suffix: repeat emoji 8 times
        char32_t emoji = get_thread_unicode_suffix(thread_id);
        for (int i = 0; i < 8; ++i) {
            pos += encode_utf8(pos, emoji);
        }
        *pos = '\0';

        return {tl_buf, static_cast<size_t>(pos - tl_buf)};
    }

    sstatic std::string_view make(int tid, int index) noexcept {
        pos = buffer;

        constexpr const char prefix[] = "payload-";
        std::memcpy(pos, prefix, sizeof(prefix) - 1);
        pos += sizeof(prefix) - 1;

        pos = itoa(pos, tid);
        *pos++ = '-';
        pos = itoa(pos, index);
        *pos++ = ' ';

        // Middle padding with '.'
        const size_t pad_len = PayloadSize - 80;  // leave room for suffix
        std::memset(pos, '.', pad_len);
        pos += pad_len;

        // Thread-specific visible Unicode suffix: box-drawing characters (highly visible)
        // Cycle through 8 distinct characters
        constexpr char32_t box_chars[8] = {0x250F, 0x2513, 0x2517, 0x251B, 0x2523, 0x252B, 0x2533, 0x253B}; // ┏ ┓ ┗ ┛ ┣ ┫ ┳ ┻
        char32_t ch = box_chars[tid % 8];

        // Repeat 12 times for visibility
        for (int i = 0; i < 12; ++i) {
            pos += encode_utf8(pos, ch);
        }

        *pos = '\0';

        return {buffer, static_cast<std::size_t>(pos - buffer)};
    }

} // end of namespace jac::ts_store::inline_v001