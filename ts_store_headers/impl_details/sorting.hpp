// ts_store/ts_store_headers/impl_details/sorting.hpp
// Optional advanced sorting — safe to include or exclude

#pragma once
#include <vector>
#include <algorithm>
#include <cstring>

inline std::vector<std::uint64_t> get_all_ids_sorted(int mode = 0) const
{
    std::shared_lock lock(data_mtx_);

    std::vector<std::uint64_t> ids;
    ids.reserve(rows_.size());
    for (const auto& [id, _] : rows_)
        ids.push_back(id);

    if (mode == 1) {
        // Sort by thread_id
        std::sort(ids.begin(), ids.end(), [this](uint64_t a, uint64_t b) {
            auto ita = rows_.find(a);
            auto itb = rows_.find(b);
            if (ita == rows_.end() || itb == rows_.end()) return false;
            return ita->second.thread_id < itb->second.thread_id;
        });
    }
    else if (mode == 2) {
        // Sort by payload string (lexicographic)
        std::sort(ids.begin(), ids.end(), [this](uint64_t a, uint64_t b) {
            auto ita = rows_.find(a);
            auto itb = rows_.find(b);
            if (ita == rows_.end() || itb == rows_.end()) return false;
            return std::strcmp(ita->second.value, itb->second.value) < 0;
        });
    }
    // mode == 0 → already sorted by ID (chronological)

    return ids;
}

// Bonus: helper if you ever want sorted timestamps
inline std::vector<std::uint64_t> get_ids_sorted_by_timestamp() const
{
    if constexpr (!Config::UseTimestamps)
        return {};

    std::shared_lock lock(data_mtx_);
    std::vector<std::uint64_t> ids;
    ids.reserve(rows_.size());
    for (const auto& [id, row] : rows_)
        if (row.ts_us != 0)
            ids.push_back(id);

    std::sort(ids.begin(), ids.end(), [this](uint64_t a, uint64_t b) {
        auto ita = rows_.find(a);
        auto itb = rows_.find(b);
        if (ita == rows_.end() || itb == rows_.end()) return false;
        return ita->second.ts_us < itb->second.ts_us;
    });

    return ids;
}