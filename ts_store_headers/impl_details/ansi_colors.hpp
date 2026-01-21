//ts_store/ts_store_headers/impl_details/ansi_colors.hpp
#pragma once

#include <string_view>

inline std::ostream& operator<<(std::ostream& os, std::string_view sv) {
    return os.write(sv.data(), static_cast<std::streamsize>(sv.size()));
}

namespace ansi {
    inline constexpr std::string_view reset          = "\033[0m";
    inline constexpr std::string_view bold           = "\033[1m";
    inline constexpr std::string_view dim            = "\033[2m";

    inline constexpr std::string_view red            = "\033[31m";
    inline constexpr std::string_view green          = "\033[32m";
    inline constexpr std::string_view yellow         = "\033[33m";
    inline constexpr std::string_view blue           = "\033[34m";
    inline constexpr std::string_view magenta        = "\033[35m";
    inline constexpr std::string_view cyan           = "\033[36m";
    inline constexpr std::string_view white          = "\033[37m";
    inline constexpr std::string_view gray           = "\033[90m";

    inline constexpr std::string_view bold_red       = "\033[1;31m";
    inline constexpr std::string_view bold_green     = "\033[1;32m";
    inline constexpr std::string_view bold_yellow    = "\033[1;33m";
    inline constexpr std::string_view bold_blue      = "\033[1;34m";
    inline constexpr std::string_view bold_magenta   = "\033[1;35m";
    inline constexpr std::string_view bold_cyan      = "\033[1;36m";
    inline constexpr std::string_view bold_white     = "\033[1;37m";

    inline constexpr std::string_view bright_red     = "\033[91m";
    inline constexpr std::string_view bright_green   = "\033[92m";
    inline constexpr std::string_view bright_yellow  = "\033[93m";
    inline constexpr std::string_view bright_blue    = "\033[94m";
    inline constexpr std::string_view bright_magenta = "\033[95m";
    inline constexpr std::string_view bright_cyan    = "\033[96m";
}