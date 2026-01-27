// ts_store/ts_store_headers/impl_details/diagnostic.hpp
// Clean diagnostics — reports size mismatch and invalid test payloads
// Updated: uses ansi:: colors, centered boxes, safe for large numbers

#pragma once
//#include "test_constants.hpp"


inline void diagnose_failures(size_t max_report = std::numeric_limits<size_t>::max()) const
{
    if (rows_.size() != expected_size()) {
        std::cout << ansi::bold_red
                  << std::format("[DIAGNOSE] SIZE MISMATCH — expected {:>10}, got {:>10}\n",
                                 expected_size(), rows_.size())
                  << ansi::reset;
        return;
    }

    struct Failure {
        std::uint64_t id;
        unsigned int thread_id;
        unsigned int event_id;
        std::string_view payload;
    };

    std::vector<Failure> failures;
    failures.reserve(std::min(rows_.size(), max_report));

    for (uint64_t id = 0; id < rows_.size(); ++id) {
        if (failures.size() >= max_report) break;

        const auto& row = rows_[id];
        std::string_view payload = row.value_storage;

        bool valid = false;
        for (const auto& msg : test_messages) {
            std::string_view expected = msg.substr(0, Config::max_payload_length);
            if (payload == expected) {
                valid = true;
                break;
            }
        }

        if (!valid) {
            failures.emplace_back(Failure{id, row.thread_id, row.event_id, payload});
        }
    }

    std::sort(failures.begin(), failures.end(),
              [](const auto& a, const auto& b) { return a.id < b.id; });

    const int w = thread_id_width();
    const int e = events_id_width();

    // Print individual failures
    for (const auto& f : failures) {
        std::cout << ansi::bold_red << "[DIAGNOSE] ID "
                  << std::format("{:>10}", f.id) << ansi::reset
                  << ansi::gray << " | " << ansi::reset
                  << "thread:" << ansi::magenta << std::format("{:>{}}", f.thread_id, w) << ansi::reset
                  << " event:" << ansi::magenta << std::format("{:>{}}", f.event_id, e) << ansi::reset
                  << ansi::gray << " |\n" << ansi::reset;

        std::cout << "              Actual   (len " << f.payload.size() << "): '"
                  << ansi::yellow << f.payload << ansi::reset << "'\n";

        std::cout << "              Expected (len " << f.payload.size() << "): '"
                  << ansi::yellow << "expected value here" << ansi::reset << "'\n\n";  // ← replace with actual expected if needed
    }

    // Summary box — centered and safe for large numbers
    constexpr int box_width = 79;  // inner width between ║ — adjust ±2 if alignment feels off

    if (failures.empty()) {
        std::string msg = std::format("ALL {} ENTRIES PASS DIAGNOSTICS", expected_size());
        std::cout << ansi::bold_green
                  << "╔═══════════════════════════════════════════════════════════════════════════════╗\n"
                  << "║" << std::format("{:^{}}", msg, box_width) << "║\n"
                  << "╚═══════════════════════════════════════════════════════════════════════════════╝\n"
                  << ansi::reset;
    } else {
        std::string header = "CORRUPTED TEST PAYLOADS";
        std::string report = std::format("REPORTED {} TEST PAYLOAD FAILURE(S)", failures.size());
        std::cout << ansi::bold_red
                  << "╔═══════════════════════════════════════════════════════════════════════════════╗\n"
                  << "║" << std::format("{:^{}}", header, box_width) << "║\n"
                  << "║" << std::format("{:^{}}", report, box_width) << "║\n"
                  << "╚═══════════════════════════════════════════════════════════════════════════════╝\n"
                  << ansi::reset;
    }
}