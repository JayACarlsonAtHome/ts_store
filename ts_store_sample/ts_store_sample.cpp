#include <iostream>  // For printout
#include <chrono>    // For steady_clock
#include "../ts_store.hpp"

int main() {  // Standalone compile

    //ts_store_sample
    ts_store<80> store(true);  // TS on (BufferSize=80 default)
    store.reserve(1000);  // Pre-alloc

    // Claim (insert with auto-ID) - twice for duration demo
    auto [ok1, id1] = store.claim(42, std::string_view("payload"));  // tid 42, value "payload"
    if (!ok1) {
        std::cout << "Insert failed (err code: " << static_cast<int>(id1) << ")" << "\n";
    }
    auto [ok2, id2] = store.claim(43, std::string_view("payload2"));  // Second insert (new ID)
    if (!ok2) {
        std::cout << "Insert failed (err code: " << static_cast<int>(id2) << ")" << "\n";
    }

    auto now = std::chrono::steady_clock::now();  // After claims for age

    // Select (lookup) first
    auto [val_ok, val] = store.select(id1);  // string_view to "payload"
    if (val_ok && val == std::string_view("payload")) {
        std::cout << "Match found: " << val << std::endl;  // Print confirmation
    }

    // Timestamp (if enabled) - first
    auto [ts_ok, ts] = store.get_timestamp(id1);  // steady_clock::time_point
    if (ts_ok) {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - ts).count();
        std::cout << "Timestamp age: " << duration << " us" << std::endl;
    }

    // Duration summary (now >0)
    store.show_duration("Store");
    return 0;
}