//
// ts_store_base.cpp
//

#include "../ts_store.hpp" // Uses BufferSize=80 default, pair<bool,...> returns


int main() {

    constexpr int Threads = 50;
    constexpr int WorkersPerThread = 50;
    constexpr int BufferSize = 100;
    constexpr bool UseTimeStamps = true;

    ts_store<Threads, WorkersPerThread, BufferSize, UseTimeStamps > store;
    store.test_run();  // When using this method:  UseTimeStamps = True, Debug = True (Set automatically)
    store.print();
}
