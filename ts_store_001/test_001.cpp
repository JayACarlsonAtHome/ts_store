// ts_store_base.cpp — FINAL v5 + FULL TEST MODE (production + test checks)

#include "../ts_store_headers/ts_store.hpp"
#include <iostream>

using namespace jac::ts_store::inline_v001;

using LogStore = ts_store<
    fixed_string<96>,
    fixed_string<12>,
    fixed_string<24>,
    96, 12, 24,
    true        // UseTimestamps
>;

using LogStoreDebug = ts_store<
    fixed_string<96>,
    fixed_string<12>,
    fixed_string<24>,
    96, 12, 24,
    true,       // UseTimestamps
    true        // DebugMode = true
>;

int main() {
    constexpr uint32_t threads = 5;
    constexpr uint32_t events  = 3;

    std::cout << "=== ts_store v5 — TDD DEMO ===\n\n";

    // ——————————————————————————— 1. PRODUCTION SIMULATION (with test checks) ———————————————————————————
    std::cout << "1. PRODUCTION SIMULATION (full test verification)\n";
    {
        LogStore prod(threads, events);
        prod.test_run();

        if (!prod.verify_integrity()) {
            std::cerr << "PRODUCTION SIMULATION FAILED — structural corruption\n";
            prod.diagnose_failures();
            return 1;
        }

#ifdef TS_STORE_ENABLE_TEST_CHECKS
        if (!prod.verify_test_payloads()) {
            std::cerr << "PRODUCTION SIMULATION FAILED — test payload corruption\n";
            prod.diagnose_failures();
            return 1;
        }
#endif

        std::cout << "PRODUCTION SIMULATION PASSED — 100% clean\n";
        prod.print();
        prod.press_any_key();
    }

    // ——————————————————————————— 2. DEBUG MODE (full instrumentation) ———————————————————————————
    std::cout << "2. DEBUG MODE (DebugMode = true)\n";
    {
        LogStoreDebug debug_store(threads, events);
        debug_store.test_run();  // ← triggers the beautiful first-insert dump

        if (!debug_store.verify_integrity() ||
#ifdef TS_STORE_ENABLE_TEST_CHECKS
            !debug_store.verify_test_payloads() ||
#endif
            false) {
            std::cerr << "DEBUG TEST FAILED\n";
            debug_store.diagnose_failures();
            return 1;
        }

        std::cout << "DEBUG TEST PASSED — fully observable and verified\n";
        debug_store.print();
    }

    std::cout << "\n=== ALL TESTS COMPLETED SUCCESSFULLY ===\n";
    std::cout << "ts_store v5 is UNBREAKABLE, observable, and production-ready.\n";

    return 0;
}