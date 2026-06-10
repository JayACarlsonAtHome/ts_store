module;

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <locale>
#include <sstream>
#include <string>

module jac.report;

namespace {

std::string format_with_separator(std::uint64_t magnitude, char sep) {
    const std::string digits = std::to_string(magnitude);
    const std::size_t n = digits.size();
    const std::size_t first_group = (n - 1) % 3 + 1;

    std::string out;
    out.reserve(n + n / 3);
    out.append(digits, 0, first_group);
    for (std::size_t i = first_group; i < n; i += 3) {
        out += sep;
        out.append(digits, i, 3);
    }
    return out;
}

std::locale number_locale() {
    if (const char* name = std::getenv("TS_STORE_NUMBER_LOCALE")) {
        if (name[0] != '\0') {
            try {
                return std::locale(name);
            } catch (...) {
            }
        }
    }
    try {
        return std::locale("");
    } catch (...) {
        return std::locale::classic();
    }
}

std::string format_with_locale(std::int64_t value) {
    std::ostringstream oss;
    oss.imbue(number_locale());
    oss << value;
    return oss.str();
}

std::string format_with_locale(std::uint64_t value) {
    std::ostringstream oss;
    oss.imbue(number_locale());
    oss << value;
    return oss.str();
}

}  // namespace

std::string format_locale_int(std::int64_t value) {
    if (const char* sep_env = std::getenv("TS_STORE_THOUSANDS_SEP")) {
        const char sep = sep_env[0];
        if (sep != '\0' && sep_env[1] == '\0') {
            if (value < 0) {
                return "-" + format_with_separator(static_cast<std::uint64_t>(-value), sep);
            }
            return format_with_separator(static_cast<std::uint64_t>(value), sep);
        }
    }
    return format_with_locale(value);
}

std::string format_locale_int(std::uint64_t value) {
    if (const char* sep_env = std::getenv("TS_STORE_THOUSANDS_SEP")) {
        const char sep = sep_env[0];
        if (sep != '\0' && sep_env[1] == '\0') {
            return format_with_separator(value, sep);
        }
    }
    return format_with_locale(value);
}

std::string format_locale_int(int value) {
    return format_locale_int(static_cast<std::int64_t>(value));
}