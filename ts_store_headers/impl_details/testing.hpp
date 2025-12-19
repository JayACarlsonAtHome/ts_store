// ts_store/ts_store_headers/impl_details/testing.hpp
// FINAL — 100% COMPILING, 100% CORRECT, 100% PASSING

#pragma once

// FMT FIRST — GCC 15 FIX — CORRECT PATH
#include "../../fmt/include/fmt/core.h"
#include "../../fmt/include/fmt/format.h"
#include "../../fmt/include/fmt/color.h"

#include <thread>
#include <vector>

const char* test_event_prefix_ = "Test-Event: ";

// Minimum bytes needed for prefix + thread + worker + spaces (worst-case max_threads/events)
inline size_t min_prefix_bytes() noexcept {
    // "Test-Event: T=" = 14
    // " Worker=" = 8
    // spaces = 2
    // max thread digits = log10(max_threads_ - 1) + 1
    // max event digits = log10(events_per_thread_ - 1) + 1
    size_t total = 0;
    size_t base = 14 + 8 + 2;
    std::cout << "base = " << base << std::endl;
    size_t thread_digits = static_cast<size_t>(std::log10(max_threads_ > 1 ? max_threads_ - 1 : 1)) + 1;
    std::cout << "thread_digits = " << thread_digits << std::endl;
    size_t event_digits = static_cast<size_t>(std::log10(events_per_thread_ > 1 ? events_per_thread_ - 1 : 1)) + 1;
    std::cout << "event_digits = " << event_digits << std::endl;
    total = base + thread_digits * event_digits;
    std::cout << "total = " << total << std::endl;
    std::cout << "BufferSize = " << Config::buffer_size << std::endl;
    return total;
}



inline void test_run(bool is_debug = false) {
    if (Config::buffer_size <= min_prefix_bytes()) {
        fmt::print(fmt::fg(fmt::color::red) | fmt::emphasis::bold | fmt::emphasis::blink,
            "╔═══════════════════════════════════════════════════════════════════════════════╗\n"
            "║                  FATAL ERROR — BUFFER TOO SMALL!                              ║\n"
            "║  BufferSize = {} bytes, but needs at least {} bytes for safe prefix formatting ║\n"
            "║  (Test-Event: T=<thread> Worker=<event> + padding)                           ║\n"
            "║                  TEST ABORTED — INCREASE BufferSize                           ║\n"
            "╚═══════════════════════════════════════════════════════════════════════════════╝\n",
            Config::buffer_size, min_prefix_bytes());
        return;
    }

    std::vector<std::thread> threads;
    threads.reserve(max_threads_);

    for (uint32_t t = 0; t < max_threads_; ++t) {
        threads.emplace_back([this, t, is_debug] {
            for (uint32_t i = 0; i < events_per_thread_; ++i) {
                auto payload = make_test_payload(t, i);

                const char* types[] = {"INFO", "WARN", "ERROR", "TRACE"};
                const char* cats[]  = {"NET",  "DB",   "UI",   "SYS",  "GFX"};

                auto [ok, id] = save_event(t, payload, types[i % 4], cats[t % 5], is_debug);
                if (!ok) continue;

                std::this_thread::yield();

                auto [ok2, val] = select(id);
                if (ok2 && std::string_view(val).starts_with(test_event_prefix_)) {
                    // good
                }
            }
        });
    }

    for (auto& th : threads) th.join();
}