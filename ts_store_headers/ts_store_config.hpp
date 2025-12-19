// ts_store_headers/ts_store_config.hpp
// FINAL — Templated config for ts_store parameters
// Defines types based on compile-time sizes
#pragma once

#include "fixed_string.hpp"  // fixed_string definition

namespace jac::ts_store::inline_v001 {

    template <
        size_t BufferSize = 100,
        size_t TypeSize = 16,
        size_t CategorySize = 32,
        bool UseTimestamps = true,
        bool DebugMode = false
    >
    struct ts_store_config {
        using ValueT = fixed_string<BufferSize>;
        using TypeT = fixed_string<TypeSize>;
        using CategoryT = fixed_string<CategorySize>;
        static constexpr size_t buffer_size = BufferSize;
        static constexpr size_t type_size = TypeSize;
        static constexpr size_t category_size = CategorySize;
        static constexpr bool use_timestamps = UseTimestamps;
        static constexpr bool debug_mode = DebugMode;
    };

}  // namespace jac::ts_store::inline_v001