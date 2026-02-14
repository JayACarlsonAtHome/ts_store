// ts_store_headers/ts_store_config.hpp

#pragma once

namespace jac::ts_store::inline_v001 {

    template <
        bool UseTimestamps = true,
        bool DebugMode = false,
        size_t MaxTypeLength     = 6,
        size_t MaxCategoryLength = 25,
        size_t MaxPayloadLength  = 100
    >
    struct ts_store_config {
        static_assert(MaxTypeLength     >=  5, "MaxTypeLength must be at least 5");
        static_assert(MaxCategoryLength >=  8, "MaxCategoryLength must be at least 8");
        static_assert(MaxPayloadLength  >= 70, "MaxPayloadLength must be at least 20");

        using ValueT     = std::string;
        using TypeT      = std::string;
        using CategoryT  = std::string;

        static constexpr bool use_timestamps = UseTimestamps;
        static constexpr bool debug_mode     = DebugMode;

        static constexpr size_t max_payload_length = MaxPayloadLength;
        static constexpr size_t max_type_length    = MaxTypeLength;
        static constexpr size_t max_category_length = MaxCategoryLength;

        static constexpr size_t utf8_length(std::string_view sv) noexcept {
            size_t len = 0;
            for (size_t i = 0; i < sv.size(); ) {
                uint8_t byte = static_cast<uint8_t>(sv[i]);
                if (byte < 0x80) ++i;
                else if ((byte & 0xE0) == 0xC0) i += 2;
                else if ((byte & 0xF0) == 0xE0) i += 3;
                else if ((byte & 0xF8) == 0xF0) i += 4;
                else ++i;  // Invalid, count as 1
                ++len;
            }
            return len;
        }

        static std::string utf8_truncate(std::string_view sv, size_t max_chars) {
            // Implementation from earlier (counts code points, truncates safely)
            std::string result;
            result.reserve(sv.size());
            size_t count = 0;
            for (size_t i = 0; i < sv.size() && count < max_chars; ) {
                uint8_t byte = static_cast<uint8_t>(sv[i]);
                size_t seq_len = 1;
                if (byte >= 0x80) {
                    if ((byte & 0xE0) == 0xC0) seq_len = 2;
                    else if ((byte & 0xF0) == 0xE0) seq_len = 3;
                    else if ((byte & 0xF8) == 0xF0) seq_len = 4;
                }
                if (i + seq_len <= sv.size()) {
                    result.append(sv.substr(i, seq_len));
                    ++count;
                }
                i += seq_len;
            }
            return result;
        }
    };

}  // namespace jac::ts_store::inline_v001