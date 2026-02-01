// ts_store/ts_store_headers/ts_store.hpp
// C++23 — GCC 15 — December 2025
#pragma once

#include "includes.hpp"

namespace jac::ts_store::inline_v001 {
// ——————————————————————— CONCEPTS ———————————————————————
template<typename Config>
class ts_store
{
private:
    struct row_data {
        size_t thread_id{0};
        size_t event_id{0};
        bool is_debug{false};
        std::string type_storage;
        std::string category_storage;
        std::string value_storage;
        std::conditional_t<Config::use_timestamps, uint64_t, std::monostate> ts_us{};
    };

    const size_t max_threads_;
    const size_t events_per_thread_;
public:
    // ——— GETTERS ———
[[nodiscard]] constexpr size_t id_width() const noexcept {
        const size_t max_id = expected_size() ? expected_size() - 1 : 0;
        if (max_id == 0) return 1;
        return static_cast<size_t>(std::log10(static_cast<double>(max_id))) + 2;
    }

    [[nodiscard]] constexpr size_t get_max_threads() const noexcept { return max_threads_; }

    [[nodiscard]] size_t thread_id_width() const noexcept {
        const size_t n = get_max_threads();
        if (n <= 1) return 2;
        size_t digits = 1;
        size_t temp = n - 1;
        while (temp >= 10) {
            temp /= 10;
            ++digits;
        }
    return digits + 1;
    }

    [[nodiscard]] constexpr size_t get_max_events() const noexcept { return events_per_thread_; }

    [[nodiscard]] size_t events_id_width() const noexcept {
    const size_t n = get_max_events();
    if (n <= 1) return 2;               // enough for "0" or "1"
    const size_t max_value = n - 1;
    // safe log10 — no undefined behavior on 0
    size_t digits = 1;
    size_t temp = max_value;
    while (temp >= 10) {
        temp /= 10;
        ++digits;
    }
    return digits + 1;  // +1 for safety / alignment
    }

    [[nodiscard]] size_t expected_size() const noexcept {
        return size_t(max_threads_) * events_per_thread_;
    }
    void clear() {
        next_id_.store(0, std::memory_order_relaxed);
        // Remove any rows_.clear() if present
    }

    explicit ts_store(size_t max_threads, size_t events_per_thread)
        : max_threads_(max_threads)
        , events_per_thread_(events_per_thread)
    {
        if (max_threads == 0 || events_per_thread == 0)
            throw std::invalid_argument("ts_store: thread/event count must be > 0");
        rows_.resize(expected_size());
        if constexpr (Config::use_timestamps) {
            const auto min_time = std::chrono::steady_clock::time_point::min();
            if (auto cur = s_epoch_base.load(std::memory_order_relaxed); cur == min_time)
                s_epoch_base.store(std::chrono::steady_clock::now(), std::memory_order_relaxed);
        }
    }

private:
    static inline std::atomic<std::chrono::steady_clock::time_point> s_epoch_base{
        std::chrono::steady_clock::time_point::min()
    };
    std::atomic<size_t> next_id_{0};
    std::vector<row_data> rows_;

public:
    static constexpr bool debug_mode_v = Config::debug_mode;
    #include "impl_details/core.hpp"
    #include "impl_details/test_constants.hpp"
    #include "impl_details/testing.hpp"
    #include "impl_details/press_to_cont.hpp"
    #include "impl_details/printing.hpp"
    #include "impl_details/duration.hpp"
    #include "impl_details/sorting.hpp"
    #include "impl_details/verify_checks.hpp"
    #include "impl_details/diagnostic.hpp"
};
}; // namespace jac::ts_store::inline_v001

