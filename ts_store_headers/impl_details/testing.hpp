// ts_store/ts_store_headers/impl_details/testing.hpp
// Updated to test type/category support — emits real types instead of debug flag

#pragma once
#include <iostream>

inline void test_run(bool is_debug = false)
{
    std::vector<std::thread> threads;
    std::atomic<int> total_successes{0};
    std::atomic<int> total_nulls{0};

    const uint32_t num_threads         = max_threads_;
    const uint32_t num_events_per_thr  = events_per_thread_;
    const uint64_t expected_total      = expected_size();

    auto worker = [this, num_events_per_thr, is_debug, &total_successes, &total_nulls](unsigned int tid)
    {
        int local_successes = 0;
        int local_nulls     = 0;

        const char* types[]     = { "INFO", "WARN", "ERROR", "TRACE" };
        const char* categories[] = { "NET", "DB", "UI", "SYS", "GFX" };

        for (uint32_t i = 0; i < num_events_per_thr; ++i)
        {
            auto payload = FastPayload<BufferSize>::make(tid, i);

            const char* type     = types[i % 4];
            const char* category = categories[tid % 5];

            auto [ok, id] = claim(tid, payload, type, category, is_debug);
            if (!ok) continue;

            std::this_thread::yield();

            auto [val_ok, val] = select(id);

            if (val_ok && val == payload)
                ++local_successes;
            else if (!val_ok)
                ++local_nulls;
        }

        total_successes += local_successes;
        total_nulls     += local_nulls;
    };

    threads.reserve(num_threads);
    for (unsigned int t = 0; t < num_threads; ++t)
        threads.emplace_back(worker, t);

    for (auto& t : threads) t.join();

    std::cout << "test_run(is_debug=" << (is_debug ? "true" : "false") << ") complete:\n"
              << "  Expected entries : " << expected_total << "\n"
              << "  Successful reads : " << total_successes << "\n"
              << "  Visibility races  : " << total_nulls << " (should be 0)\n";

    if (total_successes == static_cast<int>(expected_total) && total_nulls == 0)
        std::cout << "  PASS — 100% correct\n\n";
    else
        std::cout << "  FAIL — data loss or races detected\n\n";
}

// Convenience overloads
inline void test_run() { test_run(false); }

inline void test_run_and_print(bool is_debug = false)
{
    test_run(is_debug);
    print();
}