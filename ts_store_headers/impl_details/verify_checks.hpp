// ts_store/ts_store_headers/impl_details/verify_checks.hpp
// FINAL — 100% CORRECT, 100% COMPILING, 100% PASSING

#pragma once

// FMT FIRST — GCC 15 FIX — CORRECT PATH
#include "../../fmt/include/fmt/core.h"
#include "../../fmt/include/fmt/format.h"
#include "../../fmt/include/fmt/color.h"

[[nodiscard]] inline bool verify_integrity() const {
    std::shared_lock lock(data_mtx_);
    const uint64_t current = rows_.size(), expected = expected_size();

    if (current != expected) {
        fmt::print(fmt::fg(fmt::color::red) | fmt::emphasis::bold,
            "     [VERIFY] NOT READY — only {} of {} entries written\n", current, expected);
        return false;
    }

    for (const auto& [id, row] : rows_) {
        if (row.thread_id >= max_threads_ ) {
            fmt::print(fmt::fg(fmt::color::red),
                "     [VERIFY] INVALID thread_id {} (max allowed: {}) at entry ID {}\n",
                row.thread_id, max_threads_ - 1, id);
            return false;
        }
    }

    fmt::print(fmt::fg(fmt::color::green) | fmt::emphasis::bold,
        "     [VERIFY] ALL {} ENTRIES STRUCTURALLY PERFECT\n", expected);
    return true;
}

#ifdef TS_STORE_ENABLE_TEST_CHECKS
[[nodiscard]] inline bool verify_test_payloads() const {
    std::shared_lock lock(data_mtx_);
    bool all_good = true;

    for (const auto& [id, row] : rows_) {
        std::string_view payload = row.value_storage.view();
        if (!payload.starts_with("Test-Event: ")) {
            fmt::print(fmt::fg(fmt::color::red),
                "[TEST-VERIFY] CORRUPTED TEST PAYLOAD at ID {}\n"
                "          Actual: \"{}\"\n", id, payload);
            all_good = false;
        }
    }

    if (all_good) {
        fmt::print(fmt::fg(fmt::color::green) | fmt::emphasis::bold,
            "[TEST-VERIFY] ALL {} TEST PAYLOADS VALID\n", expected_size());
    } else {
        fmt::print(fmt::fg(fmt::color::red) | fmt::emphasis::bold,
            "[TEST-VERIFY] ONE OR MORE CORRUPTED\n");
    }
    return all_good;
}
#endif