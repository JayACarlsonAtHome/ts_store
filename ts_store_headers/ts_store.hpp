// ts_store/ts_store_headers/ts_store.hpp
// FINAL v5 — now with optional DebugMode

#pragma once

#undef _GLIBCXX_VISIBILITY
#define _GLIBCXX_VISIBILITY(...)

#include <thread>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <chrono>
#include <concepts>
#include <string>
#include <string_view>
#include <type_traits>
#include <array>
#include <vector>
#include <cstring>
#include <iomanip>
#include <sys/sysinfo.h>

#include "../GTL/include/gtl/phmap.hpp"
#include "impl_details/fast_payload.hpp"
#include "impl_details/memory_guard.hpp"

namespace jac::ts_store::inline_v001 {

// ——————————————————————— CONCEPTS ———————————————————————
template<typename T>
concept StringLike = requires(T t) {
    { std::string_view(t) } -> std::convertible_to<std::string_view>;
};

template<typename T>
concept TriviallyCopyableStringLike = std::is_trivially_copyable_v<T> && StringLike<T>;

// ——————————————————————— fixed_string — BUILT-IN ———————————————————————
template<size_t N>
struct fixed_string {
    char data[N] = {};

    constexpr fixed_string() = default;

    constexpr fixed_string(const char* s) {
        if (s) std::strncpy(data, s, N - 1);
        data[N - 1] = '\0';
    }

    constexpr fixed_string(std::string_view sv) {
        const size_t len = sv.size() < N ? sv.size() : N - 1;
        std::memcpy(data, sv.data(), len);
        data[len] = '\0';
    }

    constexpr operator std::string_view() const noexcept {
        return { data, std::strlen(data) };
    }
};

// ——————————————————————— ts_store ———————————————————————
template <
    typename ValueT,
    typename TypeT      = fixed_string<16>,
    typename CategoryT  = fixed_string<32>,
    size_t   BufferSize = 100,
    size_t   TypeSize   = 16,
    size_t   CategorySize = 32,
    bool     UseTimestamps = true,
    bool     DebugMode = false                  // ← NEW: zero-cost debug mode
>
    requires StringLike<ValueT> && StringLike<TypeT> && StringLike<CategoryT>
class ts_store {
private:
    struct row_data {
        unsigned int thread_id{0};
        bool         is_debug{false};

        [[no_unique_address]]
        std::conditional_t<std::is_same_v<TypeT, std::string_view>,
            std::array<char, TypeSize>, TypeT> type_storage{};

        [[no_unique_address]]
        std::conditional_t<std::is_same_v<CategoryT, std::string_view>,
            std::array<char, CategorySize>, CategoryT> category_storage{};

        [[no_unique_address]]
        std::conditional_t<TriviallyCopyableStringLike<ValueT>,
            ValueT, std::array<char, BufferSize>> value_storage{};

        std::conditional_t<UseTimestamps, uint64_t, std::monostate> ts_us{};
    };

    const uint32_t max_threads_;
    const uint32_t events_per_thread_;

public:
    [[nodiscard]] uint64_t expected_size() const noexcept {
        return uint64_t(max_threads_) * events_per_thread_;
    }

    void clear() {
        std::unique_lock lock(data_mtx_);
        rows_.clear();
        next_id_.store(0, std::memory_order_relaxed);
    }

    explicit ts_store(uint32_t max_threads, uint32_t events_per_thread)
        : max_threads_(max_threads)
        , events_per_thread_(events_per_thread)
    {
        if (max_threads == 0 || events_per_thread == 0)
            throw std::invalid_argument("ts_store: thread/event count must be > 0");

        rows_.reserve(expected_size() * 2);

        if constexpr (UseTimestamps) {
            const auto min_time = std::chrono::steady_clock::time_point::min();
            if (auto cur = epoch_base.load(std::memory_order_relaxed); cur == min_time)
                epoch_base.store(std::chrono::steady_clock::now(), std::memory_order_relaxed);
        }

        static const memory_guard<ValueT, TypeT, CategoryT, BufferSize, TypeSize, CategorySize, UseTimestamps>
            guard(max_threads_, events_per_thread_);
        (void)guard;
    }

private:
    static inline std::atomic<std::chrono::steady_clock::time_point> epoch_base{
        std::chrono::steady_clock::time_point::min()
    };

    std::atomic<std::uint64_t> next_id_{0};
    gtl::parallel_flat_hash_map<std::uint64_t, row_data> rows_;
    mutable std::shared_mutex data_mtx_;

public:
    // DebugMode is now available here
    static constexpr bool debug_mode_v = DebugMode;

    #include "impl_details/core.hpp"
    #include "impl_details/testing.hpp"
    #include "impl_details/printing.hpp"
    #include "impl_details/duration.hpp"
    #include "impl_details/sorting.hpp"
    #include "impl_details/press_to_cont.hpp"
};

} // namespace jac::ts_store::inline_v001