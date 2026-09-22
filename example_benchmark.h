#pragma once

#include "benchmark.h"

// Example for copying
class CopyThisBenchmark : public bench::BenchmarkGroup {
public:
    CopyThisBenchmark() : bench::BenchmarkGroup("Math & Bitwise Operations") {
        // Add any number of benchmark functions
        add_test("64-bit Multiplication", [this]() {
            volatile uint64_t a = 123456789ULL;
            volatile uint64_t b = 987654321ULL;
            uint64_t res = a * b;
            bench::do_not_optimize(res);
        });
    }
};

// Automatic self-registration of example benchmark classes
REGISTER_BENCHMARK_CLASS(CopyThisBenchmark)
