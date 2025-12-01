// test_002.cpp — 250,000 entry stress test using full verification

#include "../ts_store_headers/ts_store.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <format>

using namespace jac::ts_store::inline_v001;

template<size_t N, class... Args>
constexpr fixed_string<N> make_fixed(std::format_string<Args...> fmt, Args&&... args) {
    std::string temp = std::format(fmt, std::forward<Args>(args)...);
    fixed_string<N> result;
    const size_t copy_len = std::min(temp.size(), N - 1);
    std::memcpy(result.data, temp.data(), copy_len);
    result.data[copy_len] = '\0';
    return result;
}

using BigStore = ts_store<
    fixed_string<512>,
    fixed_string<32>,
    fixed_string<64>,
    512, 32, 64,
    true
>;

int main() {
    constexpr uint32_t num_threads       = 250;
    constexpr uint32_t events_per_thread = 1000;
    constexpr uint64_t total_entries     = uint64_t(num_threads) * events_per_thread;

    std::cout << "=== ts_store 250,000 entry test ===\n";
    std::cout << "Threads: " << num_threads << ", Events/thread: " << events_per_thread << "\n\n";

    BigStore store(num_threads, events_per_thread);

    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    for (uint32_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([t, &store] {
            for (uint32_t i = 0; i < events_per_thread; ++i) {
                auto payload = make_fixed<512>(
                    "thread:{} event:{} seq:{} time:{}",
                    t, i, i * 777, std::chrono::steady_clock::now().time_since_epoch().count()
                );

                auto type     = make_fixed<32>("STRESS");
                auto category = make_fixed<64>("PERF_TEST");

                auto [ok, id] = store.claim(t, payload, type, category);
                if (!ok) {
                    std::cerr << "[ERROR] claim failed — thread " << t << " event " << i << "\n";
                    std::abort();
                }
            }
        });
    }

    std::cout << "All threads started — joining...\n";
    for (auto& th : threads) th.join();

    std::cout << "All threads joined — running verification...\n\n";

    if (!store.verify_integrity()) {
        std::cerr << "Structural verification failed\n";
        store.diagnose_failures();
        return 1;
    }

#ifdef TS_STORE_ENABLE_TEST_CHECKS
    if (!store.verify_test_payloads()) {
        std::cerr << "Test payload verification failed\n";
        store.diagnose_failures();
        return 1;
    }
#endif

    std::cout << "All " << total_entries << " entries passed verification.\n";
    return 0;
}