// Created by Przemysław Wiewióra

#include <iostream>

#include "benchmark.h"

/**
 * Add include with benchmark here so it get registered and run
 */
//#include "benchmarks/example_benchmark.h"
//#include "benchmarks/example_benchmarks.h"
//#include "benchmarks/false_sharing_benchmarks.h"
#include "benchmarks/pointer_efficiency_benchmark.h"

int main() {
    std::cout << "==================================================================================================\n";
    std::cout << "                        CPU CYCLE EXECUTION TIME BENCHMARK RUNNER                                 \n";
    std::cout << "==================================================================================================\n";

    // Optional wait for input
    /*
    std::cout << "Press ENTER or type 'start' to run benchmarks: ";
    std::string input;
    std::getline(std::cin, input);
    */

    // Benchmark execution configuration
    constexpr bench::BenchmarkConfig config {
        .warmup_iterations = 1000,          // Warmup iterations to prime caches & branch predictors
        .measurement_iterations = 10000,    // Number of measurement samples
        .subtract_overhead = true           // Subtract cycle counter call overhead
    };

    // Run all registered benchmark classes and their functions
    bench::BenchmarkRunner::instance().start(config);

    return 0;
}
