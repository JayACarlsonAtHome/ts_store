//
// ts_store_base.cpp
//

#include "../ts_store_headers/ts_store.hpp"


int main() {

    constexpr int Threads = 8;
    constexpr int WorkersPerThread = 100;
    constexpr int BufferSize = 100;
    constexpr bool UseTimeStamps = true;

    ts_store<Threads, WorkersPerThread, BufferSize, UseTimeStamps > store;
    store.test_run();  // When using this method:  UseTimeStamps = True, Debug = True (Set automatically)
    store.print();
}
