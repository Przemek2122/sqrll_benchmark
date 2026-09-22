# CPU Cycle Benchmark Runner

A lightweight, header-only C++ micro-benchmarking framework designed to measure the execution performance of code snippets with high precision.

### How Performance is Measured
Performance is measured in **CPU cycles** using hardware counters and precision timing:
- **x86 / x86_64:** Serialized hardware timestamp counters (`__rdtsc` and `__rdtscp` paired with `_mm_lfence`) to eliminate out-of-order execution distortion.
- **Fallback (non-x86):** `clock_gettime(CLOCK_MONOTONIC_RAW)` nanosecond-precision monotonic timer.
- **Overhead compensation:** Automatic calibration and subtraction of cycle counter call overhead.
- **Optimization barriers:** `bench::do_not_optimize(...)` and `bench::clobber_memory()` to prevent compiler optimizations from stripping benchmark code.
- **Multithreading synchronization:** Low-latency spin barriers (`bench::SpinBarrier`) to ensure synchronized start across threads.

---

> [!WARNING]
> **Warning:** This framework is experimental and **not well tested**. Use with caution when relying on results for critical benchmarking.

---

## How to Add Benchmarks

Create a benchmark class inheriting from `bench::BenchmarkGroup` and register it with `REGISTER_BENCHMARK_CLASS`.

### 1. Without Multithreading (Single-Threaded)

Pass a test name and a parameterless lambda to `add_test()`:

```cpp
#pragma once
#include "benchmark.h"

class MathBenchmark : public bench::BenchmarkGroup {
public:
    MathBenchmark() : bench::BenchmarkGroup("Math Operations") {
        add_test("64-bit Multiplication", [this]() {
            volatile uint64_t a = 123456789ULL;
            volatile uint64_t b = 987654321ULL;
            uint64_t res = a * b;
            bench::do_not_optimize(res);
        });
    }
};

REGISTER_BENCHMARK_CLASS(MathBenchmark);
```

### 2. With Multithreading (Multi-Threaded)

Pass a test name, the number of threads, and a lambda taking `size_t thread_id` to `add_test()`:

```cpp
#pragma once
#include "benchmark.h"

class FalseSharingBenchmark : public bench::BenchmarkGroup {
public:
    FalseSharingBenchmark() : bench::BenchmarkGroup("False Sharing") {
        // Run with 2 concurrent threads
        add_test("2 Threads Parallel Work", 2, [this](size_t thread_id) {
            for (size_t i = 0; i < 10000; ++i) {
                data_[thread_id] += 1;
            }
            bench::do_not_optimize(data_[thread_id]);
        });
    }

    void set_up() override {
        // Optional: runs before measurement iterations
        for (auto& val : data_) {
            val = 0;
        }
    }

private:
    volatile uint64_t data_[2];
};

REGISTER_BENCHMARK_CLASS(FalseSharingBenchmark);
```

---

## Running the Benchmarks

1. Include your benchmark header in `main.cpp`:
   ```cpp
   #include "my_benchmark.h"
   ```
2. Build and run the project:
   ```bash
   cmake -B build
   cmake --build build
   ./build/benchmarks
   ```

## Sample output

```
==================================================================================================
                        CPU CYCLE EXECUTION TIME BENCHMARK RUNNER                                 
==================================================================================================

==================================================================================================
                             CPU CYCLE BENCHMARK FRAMEWORK                                        
==================================================================================================
[INFO] Registered Benchmark Classes: 1
[INFO] Warmup Iterations:            500
[INFO] Measurement Iterations:       2000
[INFO] Calibrated Timer Overhead:    29 cycles
--------------------------------------------------------------------------------------------------

>>> Class: [False Sharing] (5 test functions)
  Test Function                                            Threads  Min (Cycles)        Median       Average       Std Dev
  --------------------------------------------------------------------------------------------------------------------------
  Single thread (baseline)                                       1        271921      287768.0      296411.0      14961.98
  False Sharing (2 threads, shared cache line)                   2        388660      412429.0      697588.8     407793.51
  No Sharing / Padded (2 threads, separate cache lines)          2        285064      287801.0      290389.9       6350.84
  False Sharing (4 threads, shared cache line)                   4        398995      414888.0      423599.3      33060.53
  No Sharing / Padded (4 threads, separate cache lines)          4        286351      288021.0      291659.6       8452.51

==================================================================================================
                                  BENCHMARK COMPLETED                                             
==================================================================================================
```