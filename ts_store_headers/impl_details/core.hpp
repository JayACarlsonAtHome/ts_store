// ts_store/ts_store_headers/impl_details/core.hpp
// FINAL v6 — production-safe + test-only payload checks (updated for real test format)

#pragma once

#include <string_view>
#include <chrono>
#include <cstring>
#include <utility>
#include <limits>
#include <iostream>

// NO namespace — included inside ts_store class

// Primary type-safe claim
template<typename V = ValueT, typename T = TypeT, typename C = CategoryT>
    requires StringLike<V> && StringLike<T> && StringLike<C>
inline std::pair<bool, std::uint64_t>
claim(unsigned int thread_id,
      V&& value,
      T&& type,
      C&& category,
      bool debug = false)
{
    std::unique_lock lock(data_mtx_);
    const std::uint64_t id = next_id_.fetch_add(1, std::memory_order_relaxed);

    // ——————————————————————————————————————————————————————————————
    // DEBUG MODE: Full visibility on first insert
    // ——————————————————————————————————————————————————————————————
    if constexpr (DebugMode) {
        static bool first_insert = true;
        if (first_insert) {
            std::cout << "\n=== ts_store DEBUG MODE ACTIVATED (DebugMode = true) ===\n";
            std::cout << "Constructor-time parameters:\n";
            std::cout << "  max_threads_        = " << max_threads_ << '\n';
            std::cout << "  events_per_thread_  = " << events_per_thread_ << '\n';
            std::cout << "  expected_size()     = " << expected_size() << '\n';
            std::cout << "  sizeof(ValueT)      = " << sizeof(ValueT) << " B\n";
            std::cout << "  sizeof(TypeT)       = " << sizeof(TypeT) << " B\n";
            std::cout << "  sizeof(CategoryT)   = " << sizeof(CategoryT) << " B\n";
            std::cout << "  BufferSize          = " << BufferSize << '\n';
            std::cout << "  TypeSize            = " << TypeSize << '\n';
            std::cout << "  CategorySize        = " << CategorySize << '\n';
            std::cout << "  UseTimestamps       = " << (UseTimestamps ? "true" : "false") << '\n';

            std::string_view v_sv = value;
            std::string_view t_sv = type;
            std::string_view c_sv = category;

            std::cout << "\nFIRST INSERT (id = 0) — raw incoming data:\n";
            std::cout << "  thread_id           = " << thread_id << '\n';
            std::cout << "  value               = \"" << v_sv.substr(0, 140)
                      << (v_sv.size() > 140 ? "..." : "") << "\"\n";
            std::cout << "    → length          = " << v_sv.size()
                      << " (max allowed: " << BufferSize - 1 << ")\n";
            std::cout << "  type                = \"" << t_sv << "\" (len " << t_sv.size()
                      << ", max " << TypeSize - 1 << ")\n";
            std::cout << "  category            = \"" << c_sv << "\" (len " << c_sv.size()
                      << ", max " << CategorySize - 1 << ")\n";
            std::cout << "  → Truncation check:\n";
            std::cout << "      value   : " << (v_sv.size() + 1 > BufferSize   ? "WILL TRUNCATE" : "OK") << '\n';
            std::cout << "      type    : " << (t_sv.size() + 1 > TypeSize     ? "WILL TRUNCATE" : "OK") << '\n';
            std::cout << "      category: " << (c_sv.size() + 1 > CategorySize ? "WILL TRUNCATE" : "OK") << '\n';
            std::cout << "─────────────────────────────────────────────────────────────\n\n";

            first_insert = false;
        }
    }

    row_data row{};
    row.thread_id = thread_id;
    row.is_debug  = debug;

    // TYPE
    if constexpr (!std::is_same_v<TypeT, std::string_view>) {
        row.type_storage = TypeT(std::forward<T>(type));
    } else {
        const std::string_view sv = type;
        if (sv.size() + 1 > TypeSize) return {false, 0};
        std::memcpy(row.type_storage.data(), sv.data(), sv.size());
        row.type_storage[sv.size()] = '\0';
    }

    // CATEGORY
    if constexpr (!std::is_same_v<CategoryT, std::string_view>) {
        row.category_storage = CategoryT(std::forward<C>(category));
    } else {
        const std::string_view sv = category;
        if (sv.size() + 1 > CategorySize) return {false, 0};
        std::memcpy(row.category_storage.data(), sv.data(), sv.size());
        row.category_storage[sv.size()] = '\0';
    }

    // VALUE
    if constexpr (TriviallyCopyableStringLike<ValueT>) {
        row.value_storage = ValueT(std::forward<V>(value));
    } else {
        const std::string_view sv = value;
        if (sv.size() + 1 > BufferSize) return {false, 0};
        std::memcpy(row.value_storage.data(), sv.data(), sv.size());
        row.value_storage[sv.size()] = '\0';
    }

    // TIMESTAMP
    if constexpr (UseTimestamps || debug) {
        const auto now = std::chrono::steady_clock::now();
        auto base = epoch_base.load(std::memory_order_relaxed);
        if (base == std::chrono::steady_clock::time_point::min()) {
            auto expected = std::chrono::steady_clock::time_point::min();
            if (epoch_base.compare_exchange_strong(expected, now))
                base = now;
            else
                base = epoch_base.load(std::memory_order_relaxed);
        }
        if constexpr (UseTimestamps)
            row.ts_us = std::chrono::duration_cast<std::chrono::microseconds>(now - base).count();
    }

    rows_.insert_or_assign(id, std::move(row));
    return {true, id};
}

// ——————————————————————— Legacy overloads (unchanged) ———————————————————————
inline std::pair<bool, std::uint64_t>
claim(unsigned int thread_id, std::string_view payload, const char* type, const char* category, bool debug = false)
{
    return claim(thread_id, payload, std::string_view(type), std::string_view(category), debug);
}

inline std::pair<bool, std::uint64_t>
claim(unsigned int thread_id, int index, bool debug = false)
{
    auto payload = FastPayload<BufferSize>::make(thread_id, index);
    return claim(thread_id, payload, "DATA", "TEST", debug);
}

inline std::pair<bool, std::uint64_t>
claim(unsigned int thread_id, std::string_view payload, bool debug = false)
{
    return claim(thread_id, payload, "DATA", "UNKNOWN", debug);
}

// select() — unchanged
inline auto select(std::uint64_t id) const
{
    std::shared_lock lock(data_mtx_);
    auto it = rows_.find(id);
    if (it == rows_.end())
        return std::pair<bool, std::conditional_t<TriviallyCopyableStringLike<ValueT>, ValueT, std::string_view>>{false, {}};

    if constexpr (TriviallyCopyableStringLike<ValueT>)
        return std::pair{true, it->second.value_storage};
    else
        return std::pair{true, std::string_view(it->second.value_storage.data())};
}

// get_all_ids() — unchanged
inline std::vector<std::uint64_t> get_all_ids() const {
    std::shared_lock lock(data_mtx_);
    std::vector<std::uint64_t> ids;
    ids.reserve(rows_.size());
    for (const auto& p : rows_) ids.push_back(p.first);
    return ids;
}

// get_timestamp_us() — unchanged
inline std::pair<bool, uint64_t> get_timestamp_us(std::uint64_t id) const {
    if constexpr (!UseTimestamps) return {false, 0};
    std::shared_lock lock(data_mtx_);
    auto it = rows_.find(id);
    if (it == rows_.end()) return {false, 0};
    return {true, it->second.ts_us};
}

// ——————————————————————— PRODUCTION-SAFE INTEGRITY ———————————————————————
[[nodiscard]] inline bool verify_integrity() const {
    std::shared_lock lock(data_mtx_);

    const uint64_t current = rows_.size();
    const uint64_t expected = expected_size();

    if (current != expected) {
        std::cerr << "[VERIFY] NOT READY — only " << current
                  << " of " << expected << " entries written\n";
        return false;
    }

    for (const auto& [id, row] : rows_) {
        if (row.thread_id >= max_threads_) {
            std::cerr << "[VERIFY] INVALID thread_id " << row.thread_id
                      << " (max allowed: " << (max_threads_ - 1) << ")"
                      << " at entry ID " << id << '\n';
            return false;
        }

        if constexpr (!TriviallyCopyableStringLike<ValueT>) {
            if (row.value_storage.data[BufferSize - 1] != '\0') {
                std::cerr << "[VERIFY] PAYLOAD NOT NULL-TERMINATED at ID " << id << '\n';
                return false;
            }
        }
    }

    std::cout << "[VERIFY] ALL " << expected << " ENTRIES STRUCTURALLY PERFECT\n";
    return true;
}

// ——————————————————————— TEST-ONLY PAYLOAD FORMAT CHECK ———————————————————————
#ifdef TS_STORE_ENABLE_TEST_CHECKS
[[nodiscard]] inline bool verify_test_payloads() const {
    std::shared_lock lock(data_mtx_);

    bool all_good = true;
    for (const auto& [id, row] : rows_) {
        std::string_view payload = TriviallyCopyableStringLike<ValueT>
            ? std::string_view(row.value_storage)
            : std::string_view(row.value_storage.data, std::strlen(row.value_storage.data));

        if (payload.find("thread:") == std::string_view::npos ||
            payload.find("event:") == std::string_view::npos) {
            std::cerr << "[TEST-VERIFY] CORRUPTED TEST PAYLOAD at ID " << id << '\n'
                      << "          Expected format: \"thread:X event:Y ...\"\n"
                      << "          Actual payload   : \"" << payload << "\"\n";
            all_good = false;
        }
    }

    if (all_good) {
        std::cout << "[TEST-VERIFY] ALL " << rows_.size() << " TEST PAYLOADS HAVE CORRECT FORMAT\n";
    } else {
        std::cout << "[TEST-VERIFY] ONE OR MORE TEST PAYLOADS ARE CORRUPTED\n";
    }
    return all_good;
}
#endif

// ——————————————————————— DIAGNOSE FAILURES (test-aware) ———————————————————————
inline void diagnose_failures(size_t max_report = std::numeric_limits<size_t>::max()) const {
    std::shared_lock lock(data_mtx_);
    size_t reported = 0;

    if (rows_.size() != expected_size()) {
        std::cerr << "[DIAGNOSE] SIZE MISMATCH: expected " << expected_size()
                  << ", got " << rows_.size() << '\n';
        return;
    }

#ifdef TS_STORE_ENABLE_TEST_CHECKS
    for (const auto& [id, row] : rows_) {
        if (reported >= max_report) break;

        std::string_view payload = TriviallyCopyableStringLike<ValueT>
            ? std::string_view(row.value_storage)
            : std::string_view(row.value_storage.data, std::strlen(row.value_storage.data));

        if (payload.find("thread:") == std::string_view::npos ||
            payload.find("event:") == std::string_view::npos) {
            std::cerr << "[DIAGNOSE] ID " << id
                      << " | thread:" << row.thread_id
                      << " | payload: '" << payload << "'"
                      << " | reason: missing 'thread:' or 'event:' in payload\n";
            ++reported;
        }
    }
#endif

    if (reported == 0) {
        std::cout << "[DIAGNOSE] ALL " << rows_.size() << " ENTRIES PASS DIAGNOSTICS\n";
    } else {
        std::cout << "[DIAGNOSE] Reported " << reported << " test payload failures\n";
    }
}