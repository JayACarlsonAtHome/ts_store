// ts_store/ts_store_headers/impl_details/verify_checks.hpp
// Rewritten for std::format + ANSI colors (C++20 compatible, no fmt dependency)

#pragma once

#include <format>
#include <iostream>
#include <string_view>

[[nodiscard]] inline bool verify_integrity() const {
    std::shared_lock lock(data_mtx_);
    const uint64_t current = rows_.size(), expected = expected_size();

    if (current != expected) {
        std::cout << std::format("\033[1;31m     [VERIFY] NOT READY — only {} of {} entries written\033[0m\n",
                                 current, expected);
        return false;
    }

    for (const auto& [id, row] : rows_) {
        if (row.thread_id >= max_threads_) {
            std::cout << std::format("\033[31m     [VERIFY] INVALID thread_id {} (max allowed: {}) at entry ID {}\033[0m\n",
                                     row.thread_id, max_threads_ - 1, id);
            return false;
        }
    }

    std::cout << std::format("\033[1;32m     [VERIFY] ALL {} ENTRIES STRUCTURALLY PERFECT\033[0m\n", expected);
    return true;
}

#ifdef TS_STORE_ENABLE_TEST_CHECKS
[[nodiscard]] inline bool verify_test_payloads() const {
    std::shared_lock lock(data_mtx_);
    bool all_good = true;

    for (const auto& [id, row] : rows_) {
        std::string_view payload = row.value_storage;
        if (!payload.starts_with("Test-Event: ")) {
            std::cout << std::format("\033[31m[TEST-VERIFY] CORRUPTED TEST PAYLOAD at ID {}\033[0m\n"
                                     "          Actual: \"{}\"\n", id, payload);
            all_good = false;
        }
    }

    if (all_good) {
        std::cout << std::format("\033[1;32m[TEST-VERIFY] ALL {} TEST PAYLOADS VALID\033[0m\n", expected_size());
    } else {
        std::cout << std::format("\033[1;31m[TEST-VERIFY] ONE OR MORE CORRUPTED\033[0m\n");
    }
    return all_good;
}
#endif