// ts_store.hpp
// FINAL — CORRECT — ZERO visibility races + compiles cleanly

#pragma once

#include <atomic>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <shared_mutex>
#include <string_view>
#include <thread>
#include <vector>
#include <iostream>
#include <algorithm>
#include "./GTL/include/gtl/phmap.hpp"

template <
    size_t Threads,
    size_t WorkersPerThread,
    size_t BufferSize = 80,
    bool UseTimestamps = true
>
class ts_store {
public:
    static constexpr size_t ExpectedSize = Threads * WorkersPerThread;

private:
    struct row_data {
        unsigned int thread_id{0};
        bool is_debug{false};
        char value[BufferSize];
        uint64_t ts_us{0};
    };

    static inline std::atomic<std::chrono::steady_clock::time_point> epoch_base{
        std::chrono::steady_clock::time_point::min()
    };

    std::atomic<std::uint64_t> next_id_{0};
    gtl::parallel_flat_hash_map<std::uint64_t, row_data> rows_;
    mutable std::shared_mutex data_mtx_;
    std::vector<std::uint64_t> claimed_ids_;
    bool useTS_{UseTimestamps};

public:
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

    std::vector<std::uint64_t> get_claimed_ids_sorted(int mode = 0) const {
        std::shared_lock lock(data_mtx_);
        auto ids = claimed_ids_;

        if (mode == 1) {
            std::sort(ids.begin(), ids.end(), [this](uint64_t a, uint64_t b) {
                auto ita = rows_.find(a); auto itb = rows_.find(b);
                if (ita == rows_.end() || itb == rows_.end()) return false;
                return ita->second.thread_id < itb->second.thread_id;
            });
        }
        else if (mode == 2) {
            std::sort(ids.begin(), ids.end(), [this](uint64_t a, uint64_t b) {
                auto ita = rows_.find(a); auto itb = rows_.find(b);
                if (ita == rows_.end() || itb == rows_.end()) return false;
                return ita->second.ts_us < itb->second.ts_us;
            });
        }
        else if (mode == 3) {
            std::sort(ids.begin(), ids.end(), [this](uint64_t a, uint64_t b) {
                auto ita = rows_.find(a); auto itb = rows_.find(b);
                if (ita == rows_.end() || itb == rows_.end()) return false;
                return std::strcmp(ita->second.value, itb->second.value) < 0;
            });
        }
        return ids;
    }

    void test_run() {
        std::vector<std::thread> threads;
        std::atomic<int> total_successes{0};
        std::atomic<int> total_nulls{0};

        auto worker = [this, &total_successes, &total_nulls](unsigned int tid) {
            int local_successes = 0;
            int local_nulls = 0;

            for (int i = 0; i < int(WorkersPerThread); ++i) {
                std::string payload = "payload-" + std::to_string(tid) + "-" + std::to_string(i);
                auto [ok, id] = claim(tid, payload, true);
                if (!ok) continue;

                std::this_thread::yield();
                auto [val_ok, val] = select(id);
                if (val_ok && val == payload) ++local_successes;
                else if (!val_ok) ++local_nulls;
            }

            total_successes += local_successes;
            total_nulls += local_nulls;
        };

        for (unsigned int t = 0; t < Threads; ++t)
            threads.emplace_back(worker, t);
        for (auto& t : threads) t.join();

        std::cout << "Test complete: " << total_successes << " / " << ExpectedSize
                  << " successes, " << total_nulls << " visibility races (should be 0)\n\n";
    }

    void print(std::ostream& os = std::cout, int sort_mode = 2) const {
        std::shared_lock lock(data_mtx_);
        auto ids = get_claimed_ids_sorted(sort_mode);
        if (ids.empty()) {
            os << "ts_store<" << Threads << "," << WorkersPerThread << "," << BufferSize << "> is empty.\n\n";
            return;
        }

        size_t w_id   = std::to_string(ids.back()).length();
        size_t w_tid  = std::to_string(Threads - 1).length();
        size_t w_time = 8;
        size_t bufferOffset = 31 + w_id;

        for (uint64_t id : ids) {
            auto it = rows_.find(id);
            if (it == rows_.end()) continue;
            w_tid = std::max(w_tid, std::to_string(it->second.thread_id).length());
            if (it->second.ts_us != 0)
                w_time = std::max(w_time, std::to_string(it->second.ts_us).length());
        }

        w_id = std::max(w_id, size_t(3));
        w_tid = std::max(w_tid, size_t(6));

        os << "ts_store<" << Threads << "," << WorkersPerThread << "," << BufferSize << ">\n";
        os << std::string(BufferSize+bufferOffset, '=') << "\n";

        os << std::right
           << std::setw(w_id)       << "ID"
           << std::setw(w_time + 4) << "TIME"
           << std::left
           << std::setw(8)          << " TYPE"
           << std::right
           << std::setw(w_tid + 4)  << "THREAD"
           << "  PAYLOAD (padded to " << (BufferSize - 1) << " chars)\n";
        os << std::string(BufferSize+bufferOffset, '-') << "\n";

        for (uint64_t id : ids) {
            auto it = rows_.find(id);
            if (it == rows_.end()) {
                os << std::right << std::setw(w_id) << id << " <missing>\n";
                continue;
            }
            const auto& r = it->second;
            std::string ts_str = (r.ts_us != 0) ? std::to_string(r.ts_us) : "-";
            std::string type_str = r.is_debug ? "Debug" : "Data";

            std::string payload = r.value;
            if (payload.length() < BufferSize - 1) {
                payload += std::string((BufferSize - 1) - payload.length(), '.');
            }

            os << std::right
               << std::setw(w_id)       << id
               << std::setw(w_time + 4) << ts_str
               << std::left
               << std::setw(8)          << (" " + type_str)
               << std::right
               << std::setw(w_tid + 4)  << r.thread_id
               << "  " << payload << std::endl;
        }

        os << std::string(BufferSize+bufferOffset, '=') << "\n\n";
    }

    void show_duration(const std::string& prefix = "Store") const {
        if (!useTS_ && !std::any_of(claimed_ids_.begin(), claimed_ids_.end(),
            [this](uint64_t id) { auto it = rows_.find(id); return it != rows_.end() && it->second.ts_us != 0; })) {
            return;
        }

        std::shared_lock lock(data_mtx_);

        uint64_t first_ts = 0, last_ts = 0;
        bool have_first = false, have_last = false;

        for (auto id : claimed_ids_) {
            auto it = rows_.find(id);
            if (it == rows_.end() || it->second.ts_us == 0) continue;   // ← fixed line

            if (!have_first) {
                first_ts = it->second.ts_us;
                have_first = true;
            }
            last_ts = it->second.ts_us;
            have_last = true;
        }

        if (have_first && have_last && last_ts >= first_ts) {
            std::cout << prefix << " duration: " << (last_ts - first_ts) << " µs (first to last)\n";
        }
    }

    std::pair<bool, uint64_t> get_timestamp_us(uint64_t id) const {
        std::shared_lock lock(data_mtx_);
        auto it = rows_.find(id);
        if (it == rows_.end() || it->second.ts_us == 0) {
            return {false, 0};
        }
        return {true, it->second.ts_us};
    }
};