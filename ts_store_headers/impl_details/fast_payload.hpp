// Project: ts_store
// File Path: ts_store/ts_store_headers/impl_details/fast_payload.hpp
//
#pragma once

#include <cstddef>
#include <string_view>
#include <cstring>
#include <iostream>

template <std::size_t PayloadSize>
struct FastPayload {
    static_assert(PayloadSize >= 64, "PayloadSize too small for FastPayload");

    thread_local inline static char buffer[PayloadSize]{};
    thread_local inline static char* pos = buffer;

    static std::string_view make(int tid, int index) noexcept {
        pos = buffer;

        constexpr const char prefix[] = "payload-";
        std::memcpy(pos, prefix, 8);
        pos += 8;

        pos = itoa(pos, tid);
        *pos++ = '-';
        pos = itoa(pos, index);
        *pos++ = '\0';

        size_t len = static_cast<std::size_t>(pos - buffer - 1);
        if (1==0)
        {
            std::cout << "\n=== FASTPAYLOAD DEBUG ===\n";
            std::cout << "Thread ID : " << tid << "\n";
            std::cout << "Index     : " << index << "\n";
            std::cout << "BufferSize: " << PayloadSize << "\n";
            std::cout << "Payload   : '" << std::string_view(buffer, len) << "'\n";
            std::cout << "Length    : " << len << " (max allowed: " << PayloadSize - 1 << ")\n";
            std::cout << "Status    : ";
            if (len + 1 > PayloadSize) {
                std::cout << "OVERFLOW!\n";
            } else {
                std::cout << "OK\n";
            }
            std::cout << "========================\n";
            std::cout << "Press Enter to continue...";
            std::cin.get();  // ← PAUSES HERE
        }
        return {buffer, len};
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
        unsigned v = neg ? -static_cast<unsigned>(value) : value;

        do {
            *--t = '0' + (v % 10);
            v /= 10;
        } while (v);

        if (neg) *--t = '-';

        while (t < tmp + sizeof(tmp))
            *dst++ = *t++;

        return dst;
    }
};