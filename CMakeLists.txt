# Created by Przemysław Wiewióra

cmake_minimum_required(VERSION 4.3)

project(benchmarks)

set(CMAKE_CXX_STANDARD 23)

add_executable(benchmarks main.cpp benchmark.h
    benchmarks/example_benchmark.h
    benchmarks/example_benchmarks.h
    benchmarks/pointer_efficiency_benchmark.h
    benchmarks/likely_unlikely_med_benchmark.h
        benchmarks/false_sharing_benchmarks.h
        benchmarks/likely_unlikely_benchmark.h
        benchmarks/likely_unlikely_adv_benchmark.h
)
