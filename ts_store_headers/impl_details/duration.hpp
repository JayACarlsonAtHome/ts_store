// ts_store/ts_store_headers/impl_details/duration.hpp
// Runtime-safe, clean, and zero-overhead when timestamps are disabled

#pragma once
#include <iostream>
#include <cstdint>

inline void show_duration(const std::string& prefix = "Store") const
{
    // If timestamps are disabled at compile time → do nothing, zero cost
    if constexpr (!UseTimestamps) {
        std::cout << prefix << " duration: <timestamps disabled>\n";
        return;
    }

    std::shared_lock lock(data_mtx_);

    uint64_t first_ts = 0;
    uint64_t last_ts  = 0;
    bool found_any    = false;

    for (const auto& [id, row] : rows_) {
        if (row.ts_us == 0)
            continue;

        if (!found_any) {
            first_ts = last_ts = row.ts_us;
            found_any = true;
        } else {
            if (row.ts_us < first_ts) first_ts = row.ts_us;
            if (row.ts_us > last_ts)  last_ts  = row.ts_us;
        }
    }

    if (found_any && last_ts >= first_ts) {
        uint64_t duration_us = last_ts - first_ts;
        std::cout << prefix << " duration: " << duration_us << " µs"
                  << "  (first @ " << first_ts << " µs, last @ " << last_ts << " µs)\n";
    } else {
        std::cout << prefix << " duration: <no valid timestamps recorded>\n";
    }
}