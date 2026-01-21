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
        unsigned int thread_id{0};
        unsigned int event_id{0};   // ← new: local per-thread sequence number
        bool is_debug{false};
        std::string type_storage;
        std::string category_storage;
        std::string value_storage;  // ← will now hold only the pure message/filler
        std::conditional_t<Config::use_timestamps, uint64_t, std::monostate> ts_us{};
    };

    const uint32_t max_threads_;
    const uint32_t events_per_thread_;
public:
    // ——— GETTERS ———
[[nodiscard]] constexpr uint32_t id_width() const noexcept {
        const uint64_t max_id = expected_size() ? expected_size() - 1 : 0;
        if (max_id == 0) return 1;
        return static_cast<uint32_t>(std::log10(static_cast<double>(max_id))) + 2;
    }

    [[nodiscard]] constexpr uint32_t get_max_threads() const noexcept { return max_threads_; }

    [[nodiscard]] uint32_t thread_id_width() const noexcept {
        const uint32_t n = get_max_threads();
        if (n == 0) return 1;
        return static_cast<uint32_t>(std::log10(static_cast<double>(n - 1))) + 2;
    }

    [[nodiscard]] constexpr uint32_t get_max_events() const noexcept { return events_per_thread_; }

    [[nodiscard]] constexpr uint32_t events_id_width() const noexcept {
        const uint32_t n = get_max_events();
        if (n == 0) return 1;
        return static_cast<uint32_t>(std::log10(static_cast<double>(n - 1))) + 2;
    }

    [[nodiscard]] uint64_t expected_size() const noexcept {
        return uint64_t(max_threads_) * events_per_thread_;
    }
    void clear() {
        next_id_.store(0, std::memory_order_relaxed);
        // Remove any rows_.clear() if present
    }

    explicit ts_store(uint32_t max_threads, uint32_t events_per_thread)
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
    std::atomic<std::uint64_t> next_id_{0};
    std::vector<row_data> rows_;

public:
    static constexpr bool debug_mode_v = Config::debug_mode;
    #include "impl_details/core.hpp"
    #include "impl_details/test_constants.hpp"
    #include "impl_details/testing.hpp"
    #include "impl_details/printing.hpp"
    #include "impl_details/duration.hpp"
    #include "impl_details/sorting.hpp"
    #include "impl_details/press_to_cont.hpp"
    #include "impl_details/verify_checks.hpp"
    #include "impl_details/diagnostic.hpp"
};
}; // namespace jac::ts_store::inline_v001

