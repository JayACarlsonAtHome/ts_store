// examples/jtext_payload_benchmark.cpp
// Payload size scaling benchmark for the optimized jText persistence path.
// Uses 9 ints + 6 doubles + realistic other fields.


#include <chrono>
#include <iostream>
#include <vector>
#include <random>
#include <string>

import jac.ts_store.persistence.jtext;

using namespace jac::ts_store::inline_v001;

static std::string make_payload(size_t size) {
    std::string p;
    p.reserve(size);
    for (size_t i = 0; i < size; ++i) {
        p += char('a' + (i % 26));
    }
    return p;
}

static void run_one(const std::string& name, size_t payload_size, size_t num_events) {
    constexpr size_t INT_COUNT = 9;
    constexpr size_t DBL_COUNT = 6;

    std::cout << "=== " << name << " | payload=" << payload_size << " chars ===\n";

    JTextSplitEventLog log("JText_Payload_" + std::to_string(payload_size), INT_COUNT, DBL_COUNT);

    std::mt19937 rng(42);
    std::uniform_int_distribution<int64_t> int_dist(-100000, 100000);
    std::uniform_real_distribution<double> dbl_dist(-1000.0, 1000.0);

    std::vector<int64_t> ints(INT_COUNT);
    std::vector<double>  dbls(DBL_COUNT);

    std::string payload = make_payload(payload_size);
    std::string category = "PAYLOAD_TEST";

    auto start = std::chrono::steady_clock::now();

    for (size_t i = 0; i < num_events; ++i) {
        for (auto& v : ints) v = int_dist(rng);
        for (auto& v : dbls) v = dbl_dist(rng);

        uint64_t flags = (i % 17 == 0) ? (1ULL << 1) : 0;

        log.append_event(i, i % 250, i % 4000, flags,
                         category, payload, i * 1234,
                         ints, dbls);
    }

    log.flush();

    auto end = std::chrono::steady_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    double eps = static_cast<double>(num_events) * 1'000'000.0 / static_cast<double>(us);

    std::cout << "  " << num_events << " events in " << us << " µs\n";
    std::cout << "  Throughput: " << static_cast<long long>(eps) << " events/sec\n\n";

    log.finalize();
}

int main() {
    constexpr size_t NUM_EVENTS = 300'000;   // Reasonable size for payload scaling

    std::vector<size_t> payload_sizes = {80, 160, 512};

    std::cout << "=== jText Payload Scaling Benchmark ===\n";
    std::cout << "Events per run: " << NUM_EVENTS << " | 9 ints + 6 doubles\n\n";

    for (size_t size : payload_sizes) {
        run_one("jText", size, NUM_EVENTS);
    }

    return 0;
}
