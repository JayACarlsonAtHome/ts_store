// Standalone high-throughput test using the improved jText directly
// This proves early header + 10K auto-batching works and gives us metrics.

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

import jac.jtext.writer;

int main() {
    constexpr size_t NUM_ROWS = 100'000;

    std::cout << "=== Direct jText High-Throughput Test (10K batching) ===\n";
    std::cout << "Rows: " << NUM_ROWS << "\n\n";

    auto start = std::chrono::steady_clock::now();

    JTextWriter writer("jtext_throughput_test.jtext");
    writer.set_purpose("Throughput test with 10K auto-batching");
    writer.enable_high_throughput_batching();   // 10K default
    writer.write_header();
    writer.begin_section("Data Section");

    auto header_time = std::chrono::steady_clock::now();

    for (size_t i = 0; i < NUM_ROWS; ++i) {
        JTextEntry entry;
        entry.number = i;
        entry.level_sep = '|';
        entry.fields = {
            std::to_string(i),
            "category_" + std::to_string(i % 10),
            "payload data for row " + std::to_string(i)
        };
        writer.append_entry(entry);
    }

    auto append_time = std::chrono::steady_clock::now();

    writer.flush();
    writer.finalize();

    auto end = std::chrono::steady_clock::now();

    auto header_us = std::chrono::duration_cast<std::chrono::microseconds>(header_time - start).count();
    auto append_us = std::chrono::duration_cast<std::chrono::microseconds>(append_time - header_time).count();
    auto total_us  = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    double events_per_sec = static_cast<double>(NUM_ROWS) * 1'000'000.0 / static_cast<double>(append_us);

    std::cout << "Header write time:     " << header_us << " µs\n";
    std::cout << "Append time (10K batch): " << append_us << " µs\n";
    std::cout << "Events/sec (append):   " << events_per_sec << "\n";
    std::cout << "Total time:            " << total_us << " µs\n";
    std::cout << "\nFile written: jtext_throughput_test.jtext\n";

    return 0;
}
