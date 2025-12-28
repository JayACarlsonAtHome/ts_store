# ts_store — Ultra-Fast, Zero-Allocation, Thread-Safe Event Buffer

### Why this exists
- Simple, clean user interface — pick it up in minutes
- All the complicated concurrency/details hidden away
- Powerful backend optimized for extreme throughput and correctness

A lock-protected, timestamp-ordered in-memory event store built for massive concurrency with **guaranteed no drops or corruption**.

- Never allocates on the hot path after construction  
- Fixed-size payloads (default 100 bytes, fully configurable)  
- Optional microsecond timestamps (compile-time toggle)  
- Global chronological order via 64-bit monotonic IDs  
- Survives repeated 1,000,000-event stress tests across 250 threads  
- Backed by Google's `parallel_flat_hash_map` (fastest for this pattern)  
- Unlimited concurrent readers via `shared_mutex`

### Current Status — December 2025
**Extensively tested — zero corruption observed across all runs**  
All 10 stress tests pass 100% on g++ 15.1.1 (RHEL 9/10)  

**Not production-ready yet** — API is in flux and will change before final lock-down.

### Performance (measured 2025-12-19)
| Environment         | Compiler   | Cores | Timestamps | Writes/sec (1M events)      |
|---------------------|------------|-------|------------|-----------------------------|
|                     |            |       |            | High    | Low     | Avg     |
| RHEL 10.1 VM        | g++ 15.1.1 | 4     | On         | 1.00M   | 0.58M   | 0.81M   |
| RHEL 10.1 VM        | g++ 15.1.1 | 4     | Off        | 1.14M   | 0.69M   | 0.88M   |
| Bare metal RHEL 9.7 | g++ 15.1.1 | 20    | On         | 2.18M   | 0.96M   | 1.84M   |
| Bare metal RHEL 9.7 | g++ 15.1.1 | 20    | Off        | 2.33M   | 1.16M   | 1.90M   |

*Writes: 250 threads × 4000 events over 50 runs (real data from test_005).*
*Not Measured -- bulk validation of payload timing at end of test.

### Key Features
- Pre-flight memory availability check
- Configurable via `ts_store_config<BufferSize, TypeSize, CategorySize, UseTimestamps>`
- Human-readable relative µs timestamps (zero cost when disabled)
- Monotonic 64-bit event IDs
- Colored, sortable pretty-print output
- Full structural + payload integrity verification
- Thread-local buffers — zero heap allocation on write path

### Planned Features
- Double-buffered disk persistence
- Fast queries/filtering by Type, Category, or Payload
- Numeric values in payloads with math operations (sum, min, max, avg, etc.)
- Rollups/aggregates over Type, Category, or global
- Complex number support?

### Note
Code is actively evolving and needs more polish.  
**NOT READY FOR PRIME TIME** — this is a work in progress.

### Usage Examples
See the test files:  
- `test_001_TS.cpp` through `test_005_TS.cpp` → Timestamps enabled  
- `test_001_XS.cpp` through `test_005_XS.cpp` → Timestamps disabled

- ### New Notes...Dec 27,2025
I am going to start using both GCC and Clang, and I'll try to get both to compile my project successfully. 
Since there are more painful and less painful ways of accomplishing this, 
I will probably create a dedicated page for each compiler explaining how to install and set it up with CLion.
On a side project, I'm experimenting with a new version that achieves 13M operations per second, although it
is currently only single-threaded. It may drop to about 10M ops/second once 
it becomes multithreaded, but that would still be roughly 500% faster than the current version.
