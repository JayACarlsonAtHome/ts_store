// ts_store_base.cpp — Final, clean, beautiful test driver

#include "../ts_store_headers/ts_store.hpp"
#include <iostream>

int main() {
    constexpr uint32_t threads = 50;
    constexpr uint32_t events  = 200;

    std::cout << "=== Production mode (real types & categories) ===\n";
    ts_store<96, 12, 24, true> prod(threads, events);
    prod.test_run_and_print(false);

    prod.press_any_key();

    std::cout << "\n=== Debug mode (same types, but debug flag on) ===\n";
    ts_store<96, 12, 24, true> debug(threads, events);
    debug.test_run_and_print(true);

    return 0;
}