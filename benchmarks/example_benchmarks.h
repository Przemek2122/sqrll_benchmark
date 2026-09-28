// Created by Przemysław Wiewióra

#pragma once

#include "../benchmark.h"
#include <vector>
#include <string>
#include <cmath>
#include <bit>
#include <numeric>

// Example 1: Math and bitwise operations benchmark group
class MathBenchmark : public bench::BenchmarkGroup {
public:
    MathBenchmark() : bench::BenchmarkGroup("Math & Bitwise Operations") {
        // Add any number of benchmark functions
        add_test("64-bit Multiplication", [this]() {
            volatile uint64_t a = 123456789ULL;
            volatile uint64_t b = 987654321ULL;
            uint64_t res = a * b;
            bench::do_not_optimize(res);
        });

        add_test("64-bit Division", [this]() {
            volatile uint64_t a = 987654321098765ULL;
            volatile uint64_t b = 1234567ULL;
            uint64_t res = a / b;
            bench::do_not_optimize(res);
        });

        add_test("std::sqrt", [this]() {
            volatile double val = 123456.789;
            double res = std::sqrt(val);
            bench::do_not_optimize(res);
        });

        add_test("std::sin", [this]() {
            volatile double val = 1.2345;
            double res = std::sin(val);
            bench::do_not_optimize(res);
        });

        add_test("std::popcount (bit count)", [this]() {
            volatile uint64_t val = 0xAAAAAAAAAAAAAAAAULL;
            int res = std::popcount(val);
            bench::do_not_optimize(res);
        });
    }
};

// Example 2: Container allocations and access benchmark group
class VectorBenchmark : public bench::BenchmarkGroup {
public:
    VectorBenchmark() : bench::BenchmarkGroup("Containers & Memory (std::vector)") {
        add_test("vector push_back (no reserve)", [this]() {
            std::vector<int> vec;
            for (int i = 0; i < 64; ++i) {
                vec.push_back(i);
            }
            bench::do_not_optimize(vec.data());
        });

        add_test("vector push_back (with reserve)", [this]() {
            std::vector<int> vec;
            vec.reserve(64);
            for (int i = 0; i < 64; ++i) {
                vec.push_back(i);
            }
            bench::do_not_optimize(vec.data());
        });

        add_test("L1 cache array read", [this]() {
            uint64_t sum = 0;
            for (int i = 0; i < 64; ++i) {
                sum += buffer_[i];
            }
            bench::do_not_optimize(sum);
        });
    }

    void set_up() override {
        buffer_.resize(64);
        std::iota(buffer_.begin(), buffer_.end(), 1);
    }

private:
    std::vector<int> buffer_;
};

// Example 3: String operations benchmark group
class StringBenchmark : public bench::BenchmarkGroup {
public:
    StringBenchmark() : bench::BenchmarkGroup("std::string Operations") {
        add_test("SSO Concatenation", [this]() {
            std::string s = "Hello, ";
            s += "World!";
            bench::do_not_optimize(s);
        });

        add_test("Heap-allocating Concatenation", [this]() {
            std::string s = "A very long string that exceeds Small String Optimization buffer limit";
            s += " - and another piece of text appended to it!";
            bench::do_not_optimize(s);
        });

        add_test("std::string_view search", [this]() {
            std::string_view sv = "Sample text to search for a specific character pattern inside.";
            auto pos = sv.find('p');
            bench::do_not_optimize(pos);
        });
    }
};

// Automatic self-registration of example benchmark classes
//REGISTER_BENCHMARK_CLASS(MathBenchmark)
//REGISTER_BENCHMARK_CLASS(VectorBenchmark)
//REGISTER_BENCHMARK_CLASS(StringBenchmark)
