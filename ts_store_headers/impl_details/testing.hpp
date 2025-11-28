// Project: ts_store
// File Path: ts_store/ts_store_headers/impl_details/testing.hpp
//

void test_run() {
    std::vector<std::thread> threads;
    std::atomic<int> total_successes{0};
    std::atomic<int> total_nulls{0};

    auto worker = [this, &total_successes, &total_nulls](unsigned int tid) {
        int local_successes = 0;
        int local_nulls = 0;

        for (int i = 0; i < int(WorkersPerThread); ++i) {
            // Zero-allocation payload — uses exact BufferSize from template
            auto payload = FastPayload<BufferSize>::make(tid, i);

            auto [ok, id] = claim(tid, i, true);
            if (!ok) continue;

            std::this_thread::yield();
            auto [val_ok, val] = select(id);

            if (val_ok && val == payload)
                ++local_successes;
            else if (!val_ok)
                ++local_nulls;
        }

        total_successes += local_successes;
        total_nulls += local_nulls;
    };

    for (unsigned int t = 0; t < Threads; ++t)
        threads.emplace_back(worker, t);

    for (auto& t : threads) t.join();

    std::cout << "Test complete: " << total_successes << " / " << ExpectedSize
              << " successes, " << total_nulls << " visibility races (should be 0)\n\n";
}