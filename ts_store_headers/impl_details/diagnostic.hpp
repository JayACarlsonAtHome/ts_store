// ts_store/ts_store_headers/impl_details/diagnostic.hpp
// Clean diagnostics — reports size mismatch and invalid test payloads

#pragma once

#include <format>
#include <iostream>
#include <vector>
#include <algorithm>
#include <string_view>
#include <limits>
#include <cstdint>

#include "test_constants.hpp"

inline void diagnose_failures(size_t max_report = std::numeric_limits<size_t>::max()) const
{
    if (rows_.size() != expected_size()) {
        std::cout << std::format("\033[1;31m[DIAGNOSE] SIZE MISMATCH — expected {:>10}, got {:>10}\033[0m\n",
                                 expected_size(), rows_.size());
        return;
    }

    struct Failure {
        std::uint64_t id;
        unsigned int thread_id;
        unsigned int event_id;
        std::string_view payload;
    };

    std::vector<Failure> failures;
    failures.reserve(std::min(rows_.size(), max_report));

    for (uint64_t id = 0; id < rows_.size(); ++id) {

        const auto& row = rows_[id];
        if (failures.size() >= max_report) break;

        std::string_view payload = row.value_storage;

        bool valid = false;
        for (auto msg : test_messages) {
            std::string expected = std::string { msg.substr(0,Config::max_payload_length)};
            if (payload == std::string_view(expected)) {
                valid = true;
                break;
            }
        }

        if (!valid) {
            failures.push_back({id, row.thread_id, row.event_id, payload});
        }
    }
    std::sort(failures.begin(), failures.end(), [] (const auto& a, const auto& b) {return a.id < b.id;} );
    const int w = thread_id_width();
    const int e = events_id_width();

    for (const auto& f : failures) {
        std::cout << std::format(
                      "\033[1;31m[DIAGNOSE]\033[0m"
                      " ID \033[1;37m{:>10}\033[0m \033[90m|\033[0m"
                      " thread:\033[35m{:>{}}\033[0m"
                      " event:\033[35m{:>{}}\033[0m \033[90m|\033[0m"
                      " payload (len {}): '\033[33m{}\033[0m' \033[90m|\033[0m"
                      " reason: \033[36minvalid test payload\033[0m\n",
                      f.id, f.thread_id, w, f.event_id, e, f.payload.size(), f.payload);
    }

    if (failures.empty()) {
        std::cout << "\033[1;32m╔═══════════════════════════════════════════════════════════════════════════════╗\n"
                  << std::format("║                     ALL {:>10} ENTRIES PASS DIAGNOSTICS                     ║\n", expected_size())
                  << "╚═══════════════════════════════════════════════════════════════════════════════╝\n\033[0m";
    } else {
        std::cout << "\033[1;31m╔═══════════════════════════════════════════════════════════════════════════════╗\n"
                  << "║                           CORRUPTED TEST PAYLOADS                             ║\n"
                  << "╚═══════════════════════════════════════════════════════════════════════════════╝\n"
                  << std::format("║                  REPORTED {:>10} TEST PAYLOAD FAILURE(S)                  ║\n", failures.size())
                  << "╚═══════════════════════════════════════════════════════════════════════════════╝\n\033[0m";
    }
}