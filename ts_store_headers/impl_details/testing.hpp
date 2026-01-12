// ts_store/ts_store_headers/impl_details/testing.hpp

#pragma once

#include <array>
#include <format>
#include <string>

#include <string_view>
#include <thread>
#include <vector>


inline std::string generateTestPayload(uint32_t thread_id,
                                       uint32_t event_index,
                                       std::string_view message = "") noexcept
{
    uint32_t t_padw = thread_id_width();
    uint32_t e_padw = events_id_width();
    std::string payload = std::format("Test-Event: T={:>{}} W={:>{}}", thread_id, t_padw, event_index, e_padw);
    if (!message.empty()) {
        payload += " ";
        payload.append(message);
    }

    payload.append(80, '.');

    return payload;
}

inline void test_run(bool is_debug = false) noexcept
{
    std::vector<std::thread> threads;
    threads.reserve(max_threads_);

    constexpr std::array<const char*, 5> types = {"INFO", "WARN", "ERROR", "TRACE", "DEBUG"};
    constexpr std::array<const char*, 5> categories = {"NET", "DB", "UI", "SYS", "GFX"};
    constexpr std::array<const char*, 5> event_messages = {
        "[INFO]  Processing request",
        "[WARN]  Resource usage high",
        "[ERROR] Connection failed",
        "[TRACE] Executing detailed step: cache lookup completed in 3ms",
        "[DEBUG] Internal state: active worker threads = ???????"
    };



    for (uint32_t t = 0; t < max_threads_; ++t) {

        threads.emplace_back([this, t, is_debug, &types, &categories, &event_messages] {
            for (uint32_t i = 0; i < events_per_thread_; ++i) {

                std::string_view msg_sv = event_messages[i % event_messages.size()];
                std::string payload = generateTestPayload(t, i, msg_sv);

                std::string_view type_sv = types[i % types.size()];
                std::string_view cat_sv  = categories[t % categories.size()];

                auto [ok, id] = save_event(t, payload, type_sv, cat_sv, is_debug);
                if (!ok) continue;

                std::this_thread::yield();

                auto [ok2, val] = select(id);
                if (ok2 && std::string_view(val).starts_with("Test-Event: T=")) {
                    // payload survived round-trip — success
                }
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }
}