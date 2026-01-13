// ts_store/ts_store_headers/impl_details/verify_checks.hpp
// Rewritten for std::format + ANSI colors (C++20 compatible, no fmt dependency)

#pragma once

#include <format>
#include <iostream>
#include <string_view>
#include <algorithm>
#include <vector>
#include "test_constants.hpp"

[[nodiscard]] inline bool verify_integrity() const {
    std::shared_lock lock(data_mtx_);
    const uint64_t current = rows_.size();
    const uint64_t expected = expected_size();

    if (current != expected || next_id_.load(std::memory_order_acquire) != expected) {
        std::cout << std::format("\033[1;31m     [VERIFY] SIZE/MISMATCH — entries {} (expected {}), next_id {} (expected {})\033[0m\n",
                                 current, expected, next_id_.load(), expected);
        return false;
    }

    std::vector<std::vector<bool>> seen(max_threads_, std::vector<bool>(events_per_thread_, false));

    for (const auto& [id, row] : rows_) {
        if (row.thread_id >= max_threads_ || row.event_id >= events_per_thread_) {
            std::cout << std::format("\033[1;31m     [VERIFY] OUT-OF-RANGE — thread_id {} or event_id {} at ID {}\033[0m\n",
                                     row.thread_id, row.event_id, id);
            return false;
        }
        if (seen[row.thread_id][row.event_id]) {
            std::cout << std::format("\033[1;31m     [VERIFY] DUPLICATE (thread {}, event {}) at ID {}\033[0m\n",
                                     row.thread_id, row.event_id, id);
            return false;
        }
        seen[row.thread_id][row.event_id] = true;
    }

    std::cout << std::format("\033[1;32m     [VERIFY] ALL {} ENTRIES STRUCTURALLY PERFECT — UNIQUE PAIRS, FULL COVERAGE\033[0m\n", expected);
    return true;
}


[[nodiscard]] inline bool verify_test_payloads() const {

    std::shared_lock lock(data_mtx_);
    bool all_good = true;

    for (const auto& [id, row] : rows_) {
        bool valid = false;
        std::string pay_load = row.value_storage;
        std::string expected = std::string(test_messages[row.event_id % test_messages.size()]);
        if (expected.size() < kMaxStoredPayloadLength) expected.append(kMaxStoredPayloadLength - expected.size(), '.');
        if ((pay_load == expected) && (pay_load.size() == kMaxStoredPayloadLength))  valid = true;
        if (!valid) {
            std::cout << std::format("\033[31m[TEST-VERIFY] CORRUPTED PAYLOAD at ID:{}  Thread_ID:{}  Event_ID:{} \n"
                                     "              Actual   (len {}): {}\n"
                                     "              Expected (len {}): {}\033[0m\n",
                                  id, row.thread_id, row.event_id, pay_load.size(), pay_load, expected.size(), expected);
            all_good = false;
        }
        if (all_good) {
            std::cout << std::format("\033[1;32m[TEST-VERIFY] TEST PAYLOADS VALID  ID:{}  Thread_ID:{}  Event_ID:{} \n"
                                                "                  Actual   Len({}): {}\n"
                                                "                  Expected Len({}): {}\033[0m\n",
                                id, row.thread_id, row.event_id, pay_load.size(), pay_load, expected.size(), expected);
        } else {
            std::cout << std::format("\033[1;31m[TEST-VERIFY] ONE OR MORE CORRUPTED\033[0m\n");
        }
    }
    return all_good;
}




