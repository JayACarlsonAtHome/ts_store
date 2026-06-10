module;

#include <beman/ts_store/ts_store_headers/impl_details/format_locale.hpp>

module jac.report;

std::string format_locale_int(std::int64_t value) {
    return jac::ts_store::inline_v001::format_locale_int(value);
}

std::string format_locale_int(std::uint64_t value) {
    return jac::ts_store::inline_v001::format_locale_int(value);
}

std::string format_locale_int(int value) {
    return jac::ts_store::inline_v001::format_locale_int(value);
}