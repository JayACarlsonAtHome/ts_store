// ts_store_headers/ts_store_config.hpp

#pragma once

namespace jac::ts_store::inline_v001 {

    template <
        bool UseTimestamps = true,
        bool DebugMode = false,
        unsigned int MaxPayloadLength  = 120,
        unsigned int MaxTypeLength     = 8,
        unsigned int MaxCategoryLength = 25
    >
    struct ts_store_config {
        static_assert(MaxTypeLength >= 5,     "MaxTypeLength must be at least 5");
        static_assert(MaxCategoryLength >= 8, "MaxCategoryLength must be at least 8");
        static_assert(MaxPayloadLength  >= 20, "MaxPayloadLength must be at least 20");

        using ValueT     = std::string;
        using TypeT      = std::string;
        using CategoryT  = std::string;

        static constexpr bool use_timestamps = UseTimestamps;
        static constexpr bool debug_mode     = DebugMode;

        static constexpr unsigned int max_payload_length = MaxPayloadLength;
        static constexpr unsigned int max_type_length    = MaxTypeLength;
        static constexpr unsigned int max_category_length = MaxCategoryLength;
    };

}  // namespace jac::ts_store::inline_v001