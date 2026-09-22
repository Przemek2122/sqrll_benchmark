# Created by Przemysław Wiewióra

cmake_minimum_required(VERSION 4.3)

project(benchmarks)

set(CMAKE_CXX_STANDARD 23)

add_executable(benchmarks main.cpp benchmark.h
    example_benchmarks.h
    false_sharing_benchmarks.h
)
