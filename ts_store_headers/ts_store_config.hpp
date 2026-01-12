// ts_store_headers/ts_store_config.hpp
// Current design: fully dynamic std::string storage (no fixed sizes)

#pragma once

#include <string>

namespace jac::ts_store::inline_v001 {

    template <
        bool UseTimestamps = true,
        bool DebugMode = false
    >
    struct ts_store_config {
        using ValueT     = std::string;
        using TypeT      = std::string;
        using CategoryT  = std::string;

        static constexpr bool use_timestamps = UseTimestamps;
        static constexpr bool debug_mode     = DebugMode;
    };

}  // namespace jac::ts_store::inline_v001