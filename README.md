
Tested on Red Hat Enterprise Linux 10.1  
Intel Core Ultra 7 265 (8 threads exposed in VM)  
31 GiB RAM · 7200 RPM secondary disk

Bare-metal (20 cores) expected: **2.8–3.4 million ops/sec**

### Why this exists
You needed an event buffer that:
- never drops or corrupts data
- never allocates on the hot path after `reserve()`
- gives perfect global order via timestamps
- survives million-operation stress tests
- modified from C++23 to C++17 so real older codebases can use it easier

Most solutions fail at least one of those.  
This one doesn’t.

### Features
- Fixed-size payload (default 80 bytes, template parameter)
- Microsecond timestamps relative to first claim (human-readable, zero cost)
- `claim()` returns monotonic 64-bit ID
- Reader-writer lock → unlimited concurrent readers
- GTL `parallel_flat_hash_map` backend (the fastest thing that exists for this pattern)
- Five independent stress tests, including the infamous `final_massive_test`

### Usage
```cpp
ts_store<80> log(true);           // timestamps on
log.reserve(2'000'000);

auto [ok, id] = log.claim(thread_id, "something happened");
// id is unique, entry is globally ordered, zero heap alloc
```

### When to use it
Lock contention diagnostics</br>
Real-time telemetry / tracing</br>
Game engine event logs</br>
Fuzzing replay buffers</br>
Anything that must be correct first, fast second</br>

### When NOT to use it
Persistence</br>
Trillions of events</br>
Dynamic string lengths</br>

### Performance (measured 2025-11-25)

| Environment                         | Compiler       | Cores/Threads  | Writes/sec                                             |
|-------------------------------------|----------------|----------------|--------------------------------------------------------|
| RHEL 10.1 Virtual Machine *1        | g++ 14         | 8 threads      | 1.484 million                                          |
| Bare metal RHEL 9.7       *2        | g++ 15.1.1     | 20 threads     | 2.017 million (best of 5 runs)  ~1.60M - 1.9M typical  |


*1
┌── System Info RHEL 10.1  VM ───────────────────────────────
│ OS      : Red Hat Enterprise Linux 10.1 (Coughlan)
│ Kernel  : 6.12.0-124.13.1.el10_1.x86_64
│ CPU     : Intel(R) Core(TM) Ultra 7 265 @ 8 threads
│ Memory  : 4.9Gi/31Gi
│ Uptime  : 15 minutes
└────────────────────────────────────────────────────────────

*2
┌── System Info ────────────────────────────────────────────
│ OS      : Red Hat Enterprise Linux 9.7 (Plow)
│ Kernel  : 5.14.0-611.9.1.el9_7.x86_64
│ CPU     : Intel(R) Core(TM) Ultra 7 265 @ 20 threads
│ Memory  : 5.7Gi/61Gi
│ Uptime  : 29 minutes
└─────────────────────────────────────────────────────────


