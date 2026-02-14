//tests/ts_store_001/Test_001_TS.CPP

#include "../../include/beman/ts_store/ts_store_headers/ts_store.hpp"

using namespace jac::ts_store::inline_v001;

using LogConfig = ts_store_config<true>;
using LogxStore = ts_store<LogConfig>;

int main() {
    if (std::cin.rdbuf()->in_avail() > 0) {
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    constexpr size_t threads = 8;
    constexpr size_t events = 8;
    std::cout << ansi::blue << "=== ts_store — Simple Test 001 TS "
                               "-- Equivalent to MultiThread Hello World  ===" << ansi::reset << "\n\n";

    LogxStore prod(threads, events);
    prod.test_run(); //Test Run sets Debug == True
    if (!prod.verify_level01()) {
        std::cerr << "PRODUCTION SIMULATION FAILED — structural corruption\n";
        prod.diagnose_failures();
        return 1;
    }
    if (!prod.verify_level02()) {
        std::cerr << "PRODUCTION SIMULATION FAILED — test payload corruption\n";
        prod.diagnose_failures();
        return 1;
    }
    std::cout << "PRODUCTION SIMULATION PASSED — 100% clean\n";
    prod.press_any_key();
    prod.print(0);
    std::cout << "\n=== ALL TESTS COMPLETED SUCCESSFULLY ===\n";
    return 0;
}
