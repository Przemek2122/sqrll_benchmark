// Created by Przemysław Wiewióra

#pragma once

#include "benchmark.h"
#include <cstdint>

// Standard cache line size on x86/ARM architectures
constexpr size_t CACHE_LINE_SIZE = 64;

// Unpadded element: adjacent array items share the exact same 64-byte cache line
struct ContiguousElement {
    volatile uint64_t value = 0;
};

// Padded element: each array item is aligned to its own isolated cache line
struct alignas(CACHE_LINE_SIZE) PaddedElement {
    volatile uint64_t value = 0;
};

class FalseSharingBenchmark : public bench::BenchmarkGroup {
public:
    FalseSharingBenchmark() : bench::BenchmarkGroup("False Sharing") {
        constexpr size_t loop_count = 100000;

        // Single thread baseline
        add_test("Single thread (baseline)", [this]() {
            for (size_t i = 0; i < loop_count; ++i) {
                unpadded_[0].value = unpadded_[0].value + 1;
            }
            bench::do_not_optimize(unpadded_[0].value);
        });

        // Two threads with False Sharing (threads modify adjacent memory on the same cache line)
        add_test("False Sharing (2 threads, shared cache line)", 2, [this](size_t thread_id) {
            for (size_t i = 0; i < loop_count; ++i) {
                unpadded_[thread_id].value = unpadded_[thread_id].value + 1;
            }
            bench::do_not_optimize(unpadded_[thread_id].value);
        });

        // Two threads with Padding (threads modify independent cache lines)
        add_test("No Sharing / Padded (2 threads, separate cache lines)", 2, [this](size_t thread_id) {
            for (size_t i = 0; i < loop_count; ++i) {
                padded_[thread_id].value = padded_[thread_id].value + 1;
            }
            bench::do_not_optimize(padded_[thread_id].value);
        });

        // Four threads with False Sharing
        add_test("False Sharing (4 threads, shared cache line)", 4, [this](size_t thread_id) {
            for (size_t i = 0; i < loop_count; ++i) {
                unpadded_[thread_id].value = unpadded_[thread_id].value + 1;
            }
            bench::do_not_optimize(unpadded_[thread_id].value);
        });

        // Four threads with Padding
        add_test("No Sharing / Padded (4 threads, separate cache lines)", 4, [this](size_t thread_id) {
            for (size_t i = 0; i < loop_count; ++i) {
                padded_[thread_id].value = padded_[thread_id].value + 1;
            }
            bench::do_not_optimize(padded_[thread_id].value);
        });
    }

    void set_up() override {
        for (auto& elem : unpadded_) {
            elem.value = 0;
        }
        for (auto& elem : padded_) {
            elem.value = 0;
        }
    }

private:
    ContiguousElement unpadded_[8];
    PaddedElement padded_[8];
};

REGISTER_BENCHMARK_CLASS(FalseSharingBenchmark);
