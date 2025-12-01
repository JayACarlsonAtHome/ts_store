// impl_details/testing.hpp — MODERN v5 — NO FastPayload, NO legacy

#pragma once
#include <thread>
#include <vector>
#include <format>
#include <cstring>

inline void test_run(bool is_debug = false) {
    std::vector<std::thread> threads;
    threads.reserve(max_threads_);

    for (uint32_t t = 0; t < max_threads_; ++t) {
        threads.emplace_back([this, t, is_debug] {
            for (uint32_t i = 0; i < events_per_thread_; ++i) {
                const auto payload = []<size_t N>(uint32_t t, uint32_t i) {
                    std::string s = std::format("payload-{}-{}", t, i);
                    fixed_string<N> fs;
                    std::memcpy(fs.data, s.data(), std::min(s.size(), N-1));
                    fs.data[std::min(s.size(), N-1)] = '\0';
                    return fs;
                }.template operator()<BufferSize>(t, i);

                const char* types[] = {"INFO", "WARN", "ERROR", "TRACE"};
                const char* cats[] = {"NET", "DB", "UI", "SYS", "GFX"};

                auto [ok, id] = claim(t, payload, types[i%4], cats[t%5], is_debug);
                if (!ok) continue;

                std::this_thread::yield();

                auto [ok2, val] = select(id);
                if (ok2 && std::string_view(val).find(std::format("payload-{}-{}", t, i)) != std::string_view::npos) {
                    // good
                }
            }
        });
    }

    for (auto& th : threads) th.join();
}