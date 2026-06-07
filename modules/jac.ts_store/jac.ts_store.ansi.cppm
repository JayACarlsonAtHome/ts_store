module;

#include <iostream>
#include <string_view>

#include <beman/ts_store/ts_store_headers/ansi_colors.hpp>

export module jac.ts_store.ansi;

export namespace ansi {
    using ::ansi::colors_enabled;
    using ::ansi::reset;
    using ::ansi::bold;
    using ::ansi::dim;
    using ::ansi::red;
    using ::ansi::green;
    using ::ansi::yellow;
    using ::ansi::blue;
    using ::ansi::magenta;
    using ::ansi::cyan;
    using ::ansi::white;
    using ::ansi::gray;
    using ::ansi::bold_red;
    using ::ansi::bold_green;
    using ::ansi::bold_yellow;
    using ::ansi::bold_blue;
    using ::ansi::bold_magenta;
    using ::ansi::bold_cyan;
    using ::ansi::bold_white;
    using ::ansi::bright_red;
    using ::ansi::bright_green;
    using ::ansi::bright_yellow;
    using ::ansi::bright_blue;
    using ::ansi::bright_magenta;
    using ::ansi::bright_cyan;
}