// ts_store/ts_store_headers/impl_details/testing.hpp

#pragma once

#include <array>
#include <format>
#include <string>

#include <string_view>
#include <thread>
#include <vector>
#include "test_constants.hpp"

inline void test_run(bool is_debug = false) noexcept

{
    std::vector<std::thread> threads;
    threads.reserve(max_threads_);
    for (size_t t = 0; t < max_threads_; ++t) {
        threads.emplace_back([this, t, is_debug] {

            for (size_t i = 0; i < events_per_thread_; ++i) {

                std::string_view msg_sv = test_messages[i % test_messages.size()];
                std::string payload = std::string(msg_sv);
                std::string_view cat_sv = categories[t % categories.size()];

                uint64_t raw_flags = 0;
                raw_flags = set_user_flag(raw_flags, TsStoreFlags::UserFlag::LogConsole);
                raw_flags = set_user_flag(raw_flags, TsStoreFlags::UserFlag::KeeperRecord);
                raw_flags = set_user_flag(raw_flags, TsStoreFlags::UserFlag::HotCacheHint);
                raw_flags = set_severity(raw_flags, static_cast<TsStoreFlags::Severity>(i % 8));

                if (!payload.empty()) {
                    raw_flags = set_internal_flag(raw_flags, TsStoreFlags::InternalFlag::HasData);
                }

                auto [ok, id] = save_event(t, i, std::move(payload), raw_flags, std::string(cat_sv), is_debug);if (!ok) continue;

                std::this_thread::yield();

                auto [ok2, val] = select(id);
                if (ok2 && std::string_view(val) == payload) {
                    // payload survived round-trip — success
                }
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }
}