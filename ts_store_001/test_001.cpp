 // ts_store_001/test_001.cpp —

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

int main() {
    constexpr uint32_t threads = 5;
    constexpr uint32_t events  = 3;
    std::cout << "=== ts_store — TDD DEMO ===\n\n";
    std::cout << "PRODUCTION SIMULATION (full test verification)\n";

    LogStore prod(threads, events);
    prod.test_run();  //Test Run sets Debug == True
    if (!prod.verify_integrity()) {
        std::cerr << "PRODUCTION SIMULATION FAILED — structural corruption\n";
        prod.diagnose_failures();
        return 1;
    }
    if (!prod.verify_test_payloads()) {
        std::cerr << "PRODUCTION SIMULATION FAILED — test payload corruption\n";
        prod.diagnose_failures();
        return 1;
    }
    std::cout << "PRODUCTION SIMULATION PASSED — 100% clean\n";
    prod.press_any_key();
    prod.print();
    std::cout << "\n=== ALL TESTS COMPLETED SUCCESSFULLY ===\n";
    return 0;
}