// Created by Przemysław Wiewióra

#include <iostream>

#include "benchmark.h"

/**
 * Add include with benchmark here so it get registered and run
 */
//#include "example_benchmark.h"
//#include "example_benchmarks.h"
#include "false_sharing_benchmarks.h"

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
    const bench::BenchmarkConfig config{
        .warmup_iterations = 500,       // Warmup iterations to prime caches & branch predictors
        .measurement_iterations = 2000, // Number of measurement samples
        .subtract_overhead = true       // Subtract cycle counter call overhead
    };

    // Run all registered benchmark classes and their functions
    bench::BenchmarkRunner::instance().start(config);

    return 0;
}
