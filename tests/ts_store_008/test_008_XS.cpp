// tests/ts_store_008/test_008_XS.cpp
//
// XS variant of flag-selective persistence (smaller N via runner scaling).

#include <algorithm>
#include <array>

#include <chrono>
#include <cstdint>
#include <format>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <vector>

import jac.ts_store.impl.testing;
import jac.ts_store.persistence.jtext;
#ifdef TS_STORE_ENABLE_SQLITE_PERSIST
import jac.ts_store.persistence.sql;
#endif

using namespace jac::ts_store::inline_v001;
using namespace std::chrono;

using LogConfig = ts_store_config<true, 6, 20, 43, 9, 6, false>;
using LogxStore = ts_store<LogConfig>;

namespace {

size_t flagged_per_type(size_t total_events) {
    if (total_events >= 10'000) return 100;
    return std::max<size_t>(10, total_events / 100);
}

void print_test_purpose(size_t total, size_t keeper_n, size_t db_n) {
    std::cout << "═══════════════════════════════════════════════════════════════\n";
    std::cout << " TEST 008 XS — Flag-selective persistence (smoke scale)\n";
    std::cout << "═══════════════════════════════════════════════════════════════\n";
    std::cout << " Purpose:\n";
    std::cout << "   Same as 008 TS: KeeperRecord → jText, DatabaseEntry → SQLite,\n";
    std::cout << "   disjoint index sets, remainder in-memory on the persist path.\n\n";
    std::cout << " Plan: " << total << " events, " << keeper_n << " keeper + " << db_n
              << " database flags\n\n";
}

uint64_t flags_for_global_index(size_t gid, size_t keeper_n, size_t db_n) {
    uint64_t raw = 0;
    if (gid < keeper_n) {
        raw = set_user_flag(raw, TsStoreFlags::UserFlag::KeeperRecord);
    } else if (gid < keeper_n + db_n) {
        raw = set_user_flag(raw, TsStoreFlags::UserFlag::DatabaseEntry);
    }
    return raw;
}

} // namespace

int main(int argc, char** argv) {
#ifndef TS_STORE_ENABLE_SQLITE_PERSIST
    std::cerr << "ERROR: test 008 requires SQLite persistence (TS_STORE_ENABLE_SQLITE_PERSIST=ON)\n";
    return 1;
#endif

    auto opts = parse_test_options(argc, argv);

    const size_t threads = opts.threads > 0 ? opts.threads : 10;
    const size_t events_per_thread = opts.events_per_thread > 0 ? opts.events_per_thread : 100;
    const size_t total = threads * events_per_thread;
    const size_t keeper_n = flagged_per_type(total);
    const size_t db_n = keeper_n;

    if (std::cin.rdbuf()->in_avail() > 0) {
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    print_test_purpose(total, keeper_n, db_n);

    std::string bname = opts.base_name.empty() ? "persist" : opts.base_name;
    const std::string file_base = bname + "_keeper";
    const std::string sql_base  = bname + "_database";

    const size_t im = LogConfig::the_IntMetrics;
    const size_t dm = LogConfig::the_DblMetrics;

    auto jtext_sink = std::make_unique<JTextEventSink>(file_base, im, dm, PersistMode::KeeperOnly);
    auto sql_sink   = std::make_unique<SqlEventSink>(sql_base, im, dm, PersistMode::DatabaseOnly, false);
    JTextEventSink* jtext_raw = jtext_sink.get();
    SqlEventSink*   sql_raw   = sql_sink.get();

    auto routing_sink = std::make_unique<FlagRoutingEventSink>(
        std::move(jtext_sink), std::move(sql_sink));
    auto writer = std::make_unique<DoubleBufferedWriter>(std::move(routing_sink), 256);

    LogxStore store(threads, events_per_thread);
    store.attach_persistence(std::move(writer));

    std::array<std::string, 8> pre_payloads;
    for (size_t i = 0; i < pre_payloads.size(); ++i) {
        pre_payloads[i] = std::string(LogxStore::test_messages[i]);
    }
    std::array<std::string, 5> pre_cats;
    for (size_t i = 0; i < pre_cats.size(); ++i) {
        pre_cats[i] = std::string(LogxStore::categories[i]);
    }

    auto start = high_resolution_clock::now();

    std::vector<std::thread> workers;
    workers.reserve(threads);

    for (size_t t = 0; t < threads; ++t) {
        workers.emplace_back([&, t]() {
            for (size_t i = 0; i < events_per_thread; ++i) {
                const size_t gid = t * events_per_thread + i;
                std::string payload = pre_payloads[i % pre_payloads.size()];
                std::string cat = pre_cats[t % pre_cats.size()];

                uint64_t raw_flags = flags_for_global_index(gid, keeper_n, db_n);

                std::array<int64_t, LogConfig::the_IntMetrics> ints{};
                std::array<double, LogConfig::the_DblMetrics> dbls{};
                for (size_t k = 0; k < LogConfig::the_IntMetrics; ++k) {
                    ints[k] = static_cast<int64_t>(gid * 10 + k);
                }
                for (size_t k = 0; k < LogConfig::the_DblMetrics; ++k) {
                    dbls[k] = static_cast<double>(gid) * 0.01;
                }

                auto [ok, id] = store.save_event(
                    t, i, std::move(payload), raw_flags, std::move(cat), false, ints, dbls);
                if (!ok) {
                    std::cerr << "save_event failed t=" << t << " i=" << i << "\n";
                    std::abort();
                }
                (void)id;
            }
        });
    }

    for (auto& th : workers) th.join();

    auto end = high_resolution_clock::now();
    const auto elapsed_us = duration_cast<microseconds>(end - start).count();

    if (!store.verify_level01()) {
        std::cerr << "STRUCTURAL VERIFICATION FAILED\n";
        return 1;
    }

    store.finalize_persistence();

    const size_t file_rows = jtext_raw->main_row_count();
    const size_t sql_rows  = sql_raw->main_row_count();

    std::cout << "Persist verification:\n";
    std::cout << "  jText main rows:  " << file_rows << " (expected " << keeper_n << ")\n";
    std::cout << "  SQLite main rows: " << sql_rows  << " (expected " << db_n << ")\n";

    if (file_rows != keeper_n || sql_rows != db_n) {
        std::cerr << "FLAG ROUTING FAILED\n";
        return 1;
    }

    std::cout << "  PASS — " << (keeper_n + db_n) << " / " << total << " persisted\n\n";

    if (elapsed_us <= 0) {
        std::cout << "PASS — 0 µs\n";
        return 0;
    }

    const auto ops_per_sec = static_cast<std::uint64_t>(
        static_cast<double>(total) * 1'000'000.0 / static_cast<double>(elapsed_us) + 0.5);

    std::cout << "═══════════════════════════════════════════════════════════════\n";
    std::cout << "               FINAL RESULT — FLAG-SELECTIVE RUN\n";
    std::cout << "═══════════════════════════════════════════════════════════════\n";
    std::cout << "  Events             : " << format_locale_int(static_cast<std::uint64_t>(total)) << "\n";
    std::cout << "  Persisted total    : " << format_locale_int(static_cast<std::uint64_t>(file_rows + sql_rows)) << "\n";
    std::cout << "  Wall time          : " << format_locale_int(static_cast<std::uint64_t>(elapsed_us)) << " µs\n";
    std::cout << "  Throughput         : " << format_locale_int(ops_per_sec) << " ops/sec\n";
    std::cout << "═══════════════════════════════════════════════════════════════\n";

    return 0;
}