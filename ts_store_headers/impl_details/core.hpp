
// Project: ts_store
// File Path: ts_store/ts_store_headers/impl_details/core.hpp
//

explicit ts_store() = default;

size_t size() const {
    std::shared_lock lock(data_mtx_);
    return rows_.size();
}

void clear_claimed_ids() {
    std::unique_lock lock(data_mtx_);
    claimed_ids_.clear();
}

std::pair<bool, std::uint64_t> claim(unsigned int thread_id, std::string_view payload, bool debug = false) {
    if (payload.size() + 1 > BufferSize) return {false, 0};

    std::unique_lock lock(data_mtx_);
    std::uint64_t id = next_id_.fetch_add(1, std::memory_order_relaxed);

    row_data row{};
    row.thread_id = thread_id;
    row.is_debug = debug;

    if (useTS_ || debug) {
        auto now = std::chrono::steady_clock::now();
        auto base = epoch_base.load(std::memory_order_relaxed);
        if (base == std::chrono::steady_clock::time_point::min()) {
            auto expected = std::chrono::steady_clock::time_point::min();
            epoch_base.compare_exchange_strong(expected, now);
            base = epoch_base.load(std::memory_order_relaxed);
        }
        row.ts_us = std::chrono::duration_cast<std::chrono::microseconds>(now - base).count();
    }

    std::memcpy(row.value, payload.data(), payload.size());
    row.value[payload.size()] = '\0';

    rows_.insert_or_assign(id, std::move(row));

    // FIXED: publish claimed ID while still holding the exclusive lock
    claimed_ids_.push_back(id);

    return {true, id};
}

std::pair<bool, std::string_view> select(std::uint64_t id) const {
    std::shared_lock lock(data_mtx_);
    auto it = rows_.find(id);
    if (it == rows_.end()) return {false, {}};
    return {true, std::string_view(it->second.value)};
}

std::pair<bool, uint64_t> get_timestamp_us(uint64_t id) const {
    std::shared_lock lock(data_mtx_);
    auto it = rows_.find(id);
    if (it == rows_.end() || it->second.ts_us == 0) {
        return {false, 0};
    }
    return {true, it->second.ts_us};
}