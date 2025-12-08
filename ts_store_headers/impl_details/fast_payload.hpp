// ts_store/ts_store_headers/impl_details/fast_payload.hpp
// This file is 100% compatible with the new runtime design — NO CHANGES NEEDED!

#pragma once

#include <cstddef>
#include <string_view>
#include <cstring>

namespace jac::ts_store::inline_v001 {

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


    static std::string_view make(int tid, int index) noexcept {
        pos = buffer;

        constexpr const char prefix[] = "payload-";
        std::memcpy(pos, prefix, sizeof(prefix) - 1);
        pos += sizeof(prefix) - 1;

        pos = itoa(pos, tid);
        *pos++ = '-';
        pos = itoa(pos, index);
        *pos++ = '\0';

        return {buffer, static_cast<std::size_t>(pos - buffer - 1)};
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
};

} // end of namespace jac::ts_store::inline_v001