// BEAUTIFUL DIAGNOSE — FINAL, NO WARNINGS, NO UNUSED VARIABLES

// FMT MUST BE FIRST — THIS IS THE GCC 15 FIX
// FMT MUST BE FIRST — THIS IS THE GCC 15 FIX
#include "../../../fmt/include/fmt/core.h"
#include "../../../fmt/include/fmt/format.h"
#include "../../../fmt/include/fmt/color.h"

inline void diagnose_failures(size_t max_report = std::numeric_limits<size_t>::max()) const {
    std::shared_lock lock(data_mtx_);

    if (rows_.size() != expected_size()) {
        fmt::print(fmt::fg(fmt::color::red) | fmt::emphasis::bold,
            "[DIAGNOSE] SIZE MISMATCH — expected {:>10}, got {:>10}\n",
            expected_size(), rows_.size());
        return;
    }

#ifdef TS_STORE_ENABLE_TEST_CHECKS
    struct Failure { std::uint64_t id; unsigned int thread_id; std::string_view payload; };
    std::vector<Failure> failures;
    failures.reserve(std::min(rows_.size(), max_report));

    for (const auto& [id, row] : rows_) {
        if (failures.size() >= max_report) break;
        std::string_view payload = row.value_storage.view();
        if (!payload.starts_with(test_event_prefix)) {
            failures.push_back({id, row.thread_id, payload});
        }
    }

    std::sort(failures.begin(), failures.end(),
              [](const auto& a, const auto& b) { return a.id < b.id; });

    if (!failures.empty()) {
        fmt::print(fmt::fg(fmt::color::red) | fmt::emphasis::bold,
            "╔═══════════════════════════════════════════════════════════════════════════════╗\n"
            "║                           CORRUPTED TEST PAYLOADS                             ║\n"
            "╚═══════════════════════════════════════════════════════════════════════════════╝\n");
    }

    const int w = thread_id_width();

    for (const auto& f : failures) {
        fmt::print(
            "{} ID {:>10} {} thread:{:>{}} {} payload: '{}' {} reason: {}\n",
            fmt::styled("[DIAGNOSE]",                                   fmt::fg(fmt::color::red)     | fmt::emphasis::bold),
            fmt::styled(f.id,                                           fmt::fg(fmt::color::white)   | fmt::emphasis::bold),
            fmt::styled("|",                                            fmt::fg(fmt::color::gray)),
            fmt::styled(f.thread_id,                                    fmt::fg(fmt::color::magenta)),
            w,
            fmt::styled("|",                                            fmt::fg(fmt::color::gray)),
            fmt::styled(f.payload,                                      fmt::fg(fmt::color::yellow)),
            fmt::styled("|",                                            fmt::fg(fmt::color::gray)),
            fmt::styled("corrupted test payload",                      fmt::fg(fmt::color::cyan))
        );
    }

    if (failures.empty()) {
        fmt::print(fmt::fg(fmt::color::green) | fmt::emphasis::bold,
            "╔═══════════════════════════════════════════════════════════════════════════════╗\n"
            "║                     ALL {:>10} ENTRIES PASS DIAGNOSTICS                     ║\n"
            "╚═══════════════════════════════════════════════════════════════════════════════╝\n",
            rows_.size());
    } else {
        fmt::print(fmt::fg(fmt::color::red) | fmt::emphasis::bold,
            "╔═══════════════════════════════════════════════════════════════════════════════╗\n"
            "║                  REPORTED {:>10} TEST PAYLOAD FAILURE(S)                  ║\n"
            "╚═══════════════════════════════════════════════════════════════════════════════╝\n",
            failures.size());
    }
#endif
}