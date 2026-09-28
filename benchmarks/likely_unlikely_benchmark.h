#pragma once

#include "../benchmark.h"

// Example for copying
class LikelyUnlikelyBenchmark : public bench::BenchmarkGroup {
public:
    LikelyUnlikelyBenchmark() : bench::BenchmarkGroup("Likely & Unlikely simple operations benchmarks") {

        // Neutral test for reference
        add_test("neutral", []() {
            volatile uint64_t a = 10;
            volatile uint64_t b = 10;
            uint64_t res = 0;

            if ((a * b) == 100)  {
                // Do anything
                res = a + b;

                if ((a * b) == 100)  {
                    // Do anything
                    res += a + b;
                } else {
                    // Do anything
                    res += a + b;
                }
            } else {
                // Do anything
                res = a + b;

                if ((a * b) == 100)  {
                    // Do anything
                    res += a + b;
                } else {
                    // Do anything
                    res += a + b;
                }
            }

            bench::do_not_optimize(res);
        });

        // Neutral test for reference
        add_test("likely", []() {
            volatile uint64_t a = 10;
            volatile uint64_t b = 10;
            uint64_t res = 0;

            if ((a * b) == 100) [[likely]] {
                // Do anything
                res = a + b;

                if ((a * b) == 100) [[likely]] {
                    // Do anything
                    res += a + b;
                } else {
                    // Do anything
                    res += a + b;
                }
            } else {
                // Do anything
                res = a + b;

                if ((a * b) == 100) [[likely]] {
                    // Do anything
                    res += a + b;
                } else {
                    // Do anything
                    res += a + b;
                }
            }

            bench::do_not_optimize(res);
        });

        add_test("unlikley", []() {
            volatile uint64_t a = 10;
            volatile uint64_t b = 10;
            uint64_t res = 0;

            if ((a * b) == 100)  {
                // Do anything
                res = a + b;

                if ((a * b) == 100)  {
                    // Do anything
                    res += a + b;
                } else [[unlikely]] {
                    // Do anything
                    res += a + b;
                }
            } else [[unlikely]] {
                // Do anything
                res = a + b;

                if ((a * b) == 100)  {
                    // Do anything
                    res += a + b;
                } else [[unlikely]] {
                    // Do anything
                    res += a + b;
                }
            }

            bench::do_not_optimize(res);
        });

        add_test("incorrect (wrong branch for likely)", []() {
            volatile uint64_t a = 10;
            volatile uint64_t b = 10;
            uint64_t res = 0;

            if ((a * b) == 100)  {
                // Do anything
                res = a + b;

                if ((a * b) == 100)  {
                    // Do anything
                    res += a + b;
                } else [[likely]] {
                    // Do anything
                    res += a + b;
                }
            } else [[likely]] {
                // Do anything
                res = a + b;
            }

            bench::do_not_optimize(res);
        });
    }
};

// Automatic self-registration of example benchmark classes
REGISTER_BENCHMARK_CLASS(LikelyUnlikelyBenchmark)
