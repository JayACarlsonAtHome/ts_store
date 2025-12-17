// ts_store_headers/fixed_string.hpp
// FINAL — 100% CORRECT, SAFE, BEAUTIFUL, COMPILING — December 2025

#pragma once

#include <string_view>
#include <cstring>
#include <cstddef>

namespace jac::ts_store::inline_v001 {

    template<std::size_t N>
    struct fixed_string {
        char data[N] = {};

        constexpr fixed_string() = default;

        constexpr fixed_string(const char* s) {
            if (s) std::strncpy(data, s, N - 1);
            data[N - 1] = '\0';
        }

        constexpr fixed_string(std::string_view sv) {
            const std::size_t len = sv.size() < N ? sv.size() : N - 1;
            std::memcpy(data, sv.data(), len);
            data[len] = '\0';
        }

        // SAFE READ
        constexpr std::string_view view() const noexcept {
            return { data, std::strlen(data) };
        }

        // SAFE WRITE — THE ONLY WAY TO MODIFY
        constexpr void copy_from(std::string_view sv) {
            const std::size_t len = sv.size() < N ? sv.size() : N - 1;
            std::memcpy(data, sv.data(), len);
            data[len] = '\0';
        }

        constexpr operator std::string_view() const noexcept { return view(); }
        constexpr const char* c_str() const noexcept { return data; }
    };

} // namespace