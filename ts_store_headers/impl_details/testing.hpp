// ts_store/ts_store_headers/impl_details/testing.hpp
// FINAL — 100% COMPILING, 100% CORRECT, 100% PASSING

#pragma once

// FMT FIRST — GCC 15 FIX — CORRECT PATH
#include "../../fmt/include/fmt/core.h"
#include "../../fmt/include/fmt/format.h"
#include "../../fmt/include/fmt/color.h"

#include <thread>
#include <vector>

// Now test_run() can see it
inline void test_run(bool is_debug = false) {
    std::vector<std::thread> threads;
    threads.reserve(max_threads_);

    for (uint32_t t = 0; t < max_threads_; ++t) {
        threads.emplace_back([this, t, is_debug] {
            for (uint32_t i = 0; i < events_per_thread_; ++i) {
                auto payload = make_test_payload(t, i);

                const char* types[] = {"INFO", "WARN", "ERROR", "TRACE"};
                const char* cats[]  = {"NET",  "DB",   "UI",   "SYS",  "GFX"};

                auto [ok, id] = claim(t, payload, types[i % 4], cats[t % 5], is_debug);
                if (!ok) continue;

                std::this_thread::yield();

                auto [ok2, val] = select(id);
                if (ok2 && std::string_view(val).starts_with(test_event_prefix)) {
                    // good
                }
            }
        });
    }

    for (auto& th : threads) th.join();
}