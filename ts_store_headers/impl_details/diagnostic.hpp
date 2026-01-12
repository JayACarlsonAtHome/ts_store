// ts_store/ts_store_headers/impl_details/diagnostic.hpp
// BEAUTIFUL DIAGNOSE — Updated for std::format + ANSI colors (no fmt dependency)

#pragma once

#include <format>
#include <iostream>
#include <vector>
#include <algorithm>
#include <string_view>
#include <limits>
#include <cstdint>

public:
constexpr static std::string_view test_event_prefix_ = "Test-Event: ";

inline void diagnose_failures(size_t max_report = std::numeric_limits<size_t>::max()) {
    std::shared_lock lock(data_mtx_);

    if (rows_.size() != expected_size()) {
        std::cout << std::format("\033[1;31m[DIAGNOSE] SIZE MISMATCH — expected {:>10}, got {:>10}\033[0m\n",
                                 expected_size(), rows_.size());
        return;
    }

#ifdef TS_STORE_ENABLE_TEST_CHECKS
    struct Failure {
        std::uint64_t id;
        unsigned int thread_id;
        std::string_view payload;
    };
    std::vector<Failure> failures;
    failures.reserve(std::min(rows_.size(), max_report));

    for (const auto& [id, row] : rows_) {
        if (failures.size() >= max_report) break;
        std::string_view payload = row.value_storage;
        if (!payload.starts_with(test_event_prefix_)) {
            failures.push_back({id, row.thread_id, payload});
        }
    }

    std::sort(failures.begin(), failures.end(),
              [](const auto& a, const auto& b) { return a.id < b.id; });

    if (!failures.empty()) {
        std::cout << "\033[1;31m"
                  << "╔═══════════════════════════════════════════════════════════════════════════════╗\n"
                  << "║                           CORRUPTED TEST PAYLOADS                             ║\n"
                  << "╚═══════════════════════════════════════════════════════════════════════════════╝\n"
                  << "\033[0m";
    }

    const int w = thread_id_width();

    for (const auto& f : failures) {
        std::cout << std::format("\033[1;31m[DIAGNOSE]\033[0m ID \033[1;37m{:>10}\033[0m \033[90m|\033[0m thread:\033[35m{:>{}}\033[0m \033[90m|\033[0m payload: '\033[33m{}\033[0m' \033[90m|\033[0m reason: \033[36mcorrupted test payload\033[0m\n",
                                 f.id, f.thread_id, w, f.payload);
    }

    if (failures.empty()) {
        std::cout << "\033[1;32m"
                  << "╔═══════════════════════════════════════════════════════════════════════════════╗\n"
                  << std::format("║                     ALL {:>10} ENTRIES PASS DIAGNOSTICS                     ║\n", rows_.size())
                  << "╚═══════════════════════════════════════════════════════════════════════════════╝\n"
                  << "\033[0m";
    } else {
        std::cout << "\033[1;31m"
                  << "╔═══════════════════════════════════════════════════════════════════════════════╗\n"
                  << std::format("║                  REPORTED {:>10} TEST PAYLOAD FAILURE(S)                  ║\n", failures.size())
                  << "╚═══════════════════════════════════════════════════════════════════════════════╝\n"
                  << "\033[0m";
    }
#endif
}