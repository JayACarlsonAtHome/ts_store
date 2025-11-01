// ts_safe.hpp - Zero-alloc version:
// No std::expected; uses pair<bool, T> + int error_code
// Changes: claim/select/etc now return pair<bool, T> (true=success, T=result) or pair<bool, int> for errors.
// Usage: auto [ok, id] = claim(...); if (!ok) { int err = id; /* handle */ }
// UPDATED: Default BufferSize to 80 per request.

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <vector>
#include <iostream>
#include <iomanip>
#include <string_view>  // For std::string_view returns
#include <cstring>      // For strcpy/memcpy/strcmp
#include <utility>      // For std::pair

#include "./GTL/include/gtl/phmap.hpp"

enum class ErrorCode { Ok = 0, NotFound = 1, TooLong = 2 };

template <size_t BufferSize = 80>  // CHANGED: Now size_t template param (default 80); no T
class ts_store {
private:
    struct row_data {
        int thread_id;
        char value[BufferSize];  // Fixed-size char buffer (null-terminated)
        std::chrono::time_point<std::chrono::steady_clock> ts{ std::chrono::steady_clock::time_point::min() };
    };
    std::atomic<std::uint64_t> next_id_{ 0 };
    gtl::parallel_flat_hash_map<std::uint64_t, row_data> rows_;
    mutable std::shared_mutex data_mtx_;        // Still needs protection for writes
    std::vector<std::uint64_t> claimed_ids_;

    bool useTS_{ false };

public:
    ts_store(bool use_ts = false) : useTS_(use_ts) {}
    bool use_ts() const { return useTS_; }
    void reserve(size_t n) { rows_.reserve(n); }

    std::pair<bool, std::uint64_t> claim(int thread_id, std::string_view payload) {  // pair<bool, uint64_t> (id on success)
        if (payload.size() + 1 > BufferSize) {
            return { false, static_cast<std::uint64_t>(static_cast<int>(ErrorCode::TooLong)) };
        }
        std::unique_lock<std::shared_mutex> lock(data_mtx_);
        auto id = next_id_.fetch_add(1, std::memory_order_seq_cst);
        row_data row{ thread_id };
        if (useTS_) {
            row.ts = std::chrono::steady_clock::now();
        }
        // Copy to fixed buffer (null-terminated)
        std::memcpy(row.value, payload.data(), payload.size());
        row.value[payload.size()] = '\0';
        rows_.insert_or_assign(id, std::move(row));
        claimed_ids_.push_back(id);
        std::atomic_thread_fence(std::memory_order_release);
        return { true, id };
    }

    std::pair<bool, std::pair<std::uint64_t, std::string_view>> claim_and_get(int thread_id, std::string_view payload) {  // Nested pair
        auto [ok, id_or_err] = claim(thread_id, payload);
        if (!ok) return { false, {0, {}} };  // Simplified error (id=0)
        auto [val_ok, val] = select(id_or_err);
        if (!val_ok) return { false, {id_or_err, {}} };
        return { true, {id_or_err, val} };
    }

    std::pair<bool, std::string_view> select(std::uint64_t id) const {  // pair<bool, string_view>
        std::shared_lock<std::shared_mutex> lock(data_mtx_);
        std::atomic_thread_fence(std::memory_order_acquire);
        auto it = rows_.find(id);
        if (it == rows_.end()) {
            return { false, {} };
        }
        return { true, std::string_view(it->second.value) };
    }

    std::pair<bool, bool> erase(std::uint64_t id) {  // pair<bool, bool> (true=true, false=not_found)
        std::unique_lock<std::shared_mutex> lock(data_mtx_);
        bool erased = rows_.erase(id) > 0;
        return { erased, erased };  // Simplified
    }
    size_t size() const {
        std::shared_lock<std::shared_mutex> lock(data_mtx_);
        return rows_.size();
    }

    std::pair<bool, std::chrono::time_point<std::chrono::steady_clock>> get_timestamp(std::uint64_t id) const {  // pair
        std::shared_lock<std::shared_mutex> lock(data_mtx_);
        std::atomic_thread_fence(std::memory_order_acquire);
        auto it = rows_.find(id);
        if (it == rows_.end() || !useTS_ || it->second.ts == std::chrono::steady_clock::time_point::min()) {
            return { false, {} };
        }
        return { true, it->second.ts };
    }

    void show_duration(const std::string& prefix) const {
        if (!useTS_) return;
        auto [min_ok, min_ts] = get_timestamp(0);
        auto [max_ok, max_ts] = get_timestamp(size() - 1);
        if (min_ok && max_ok) {
            auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(max_ts - min_ts).count();
            std::cout << prefix << " " << std::left << std::setw(20) << "Claim duration (us):" << std::right << std::setw(8) << duration_us << " (first to last)" << "\n";
        }
    }
    void clear_claimed_ids() { claimed_ids_.clear(); }

    std::vector<std::uint64_t> get_claimed_ids_sorted(int mode = 0) const {

        std::shared_lock<std::shared_mutex> lock(data_mtx_);

        auto ids = claimed_ids_;
        if (mode == 1) { // By tid
            std::sort(ids.begin(), ids.end(), [this](uint64_t a, uint64_t b) {
                auto ita = rows_.find(a);
                auto itb = rows_.find(b);
                if (ita == rows_.end() || itb == rows_.end()) return false;
                return ita->second.thread_id < itb->second.thread_id;
                });
        }
        else if (mode == 2) { // By time
            std::sort(ids.begin(), ids.end(), [this](uint64_t a, uint64_t b) {
                auto ita = rows_.find(a);
                auto itb = rows_.find(b);
                if (ita == rows_.end() || itb == rows_.end()) return false;
                return ita->second.ts < itb->second.ts;
                });
        }
        else if (mode == 3) { // By value
            std::sort(ids.begin(), ids.end(), [this](uint64_t a, uint64_t b) {
                auto ita = rows_.find(a);
                auto itb = rows_.find(b);
                if (ita == rows_.end() || itb == rows_.end()) return false;
                return std::strcmp(ita->second.value, itb->second.value) < 0;
                });
        } // Default: by ID
        return ids;
    }
};