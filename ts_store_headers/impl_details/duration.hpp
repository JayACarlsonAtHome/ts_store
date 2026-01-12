// ts_store/ts_store_headers/impl_details/duration.hpp
// Runtime-safe, clean, and zero-overhead when timestamps are disabled
// Updated for std::format (no fmt dependency)

#pragma once

#include <format>
#include <iostream>
#include <cstdint>
#include <string>

inline void show_duration(const std::string& name) const {
    if constexpr (Config::use_timestamps) {
        uint64_t first_ts = 0;
        uint64_t last_ts  = 0;

        for (const auto& [id, row] : rows_) {
            if (row.ts_us == 0) continue;

            if (first_ts == 0 || row.ts_us < first_ts) first_ts = row.ts_us;
            if (row.ts_us > last_ts)                   last_ts  = row.ts_us;
        }

        if (first_ts == 0) {
            std::cout << std::format("{} duration: no timed entries\n", name);
        } else {
            auto duration_us = last_ts - first_ts;
            std::cout << std::format("{} duration: {} µs ({} → {})\n", name, duration_us, first_ts, last_ts);
        }
    } else {
        std::cout << std::format("{} duration: timestamps disabled (UseTimestamps=false)\n", name);
    }
}