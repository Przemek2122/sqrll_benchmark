#pragma once

#include <cstdint>
#include <random>
#include <vector>

#include "../benchmark.h"

class LikelyUnlikelyMediumBenchmark : public bench::BenchmarkGroup {
private:
    static constexpr int kSize = 65536;  // big, so the CPU can't memorize the pattern

    std::vector<int>      chances;  // random numbers 0..99
    std::vector<uint64_t> values;   // random data to work on
    int hotPercent = 0;             // how often the hot branch runs

    // Rare, "expensive" work done only in the cold branch.
    static uint64_t cold_work(uint64_t x)
    {
        x ^= x >> 33;
        x *= 0xff51afd7ed558ccdULL;
        x ^= x >> 29;
        x *= 0xc4ceb9fe1a85ec53ULL;
        x ^= x >> 31;
        x *= 0x9e3779b97f4a7c15ULL;
        x ^= x >> 27;
        return x;
    }

public:
    void set_up() override
    {
        hotPercent = 99;

        std::mt19937_64 gen{12345};
        std::uniform_int_distribution<int>      chanceDist(0, 99);
        std::uniform_int_distribution<uint64_t> valueDist;

        chances.clear();
        values.clear();

        for (int i = 0; i < kSize; i++)
        {
            chances.push_back(chanceDist(gen));
            values.push_back(valueDist(gen));
        }
    }

    LikelyUnlikelyMediumBenchmark() : bench::BenchmarkGroup("Likely & Unlikely cold-first benchmarks") {

        // Compiler's "bad guess": rare code first, no hints.
        add_test("neutral (cold path first, no hints)", [this]() {
            uint64_t res = 0;

            for (int i = 0; i < kSize; i++)
            {
                if (chances[i] >= hotPercent) {
                    res += cold_work(values[i]);    // cold path (~1%)
                } else {
                    res += values[i];               // hot path (~99%)
                }
            }

            bench::do_not_optimize(res);
        });

        // Same code, hint on the rare branch.
        add_test("[[unlikely]] on cold path", [this]() {
            uint64_t res = 0;

            for (int i = 0; i < kSize; i++)
            {
                if (chances[i] >= hotPercent) [[unlikely]] {
                    res += cold_work(values[i]);
                } else {
                    res += values[i];
                }
            }

            bench::do_not_optimize(res);
        });

        // Same code, hint on the hot branch instead.
        add_test("[[likely]] on hot path", [this]() {
            uint64_t res = 0;

            for (int i = 0; i < kSize; i++)
            {
                if (chances[i] >= hotPercent) {
                    res += cold_work(values[i]);
                } else [[likely]] {
                    res += values[i];
                }
            }

            bench::do_not_optimize(res);
        });

        // Same code, wroong hint.
        add_test("WRONG! [[likely]]", [this]() {
            uint64_t res = 0;

            for (int i = 0; i < kSize; i++)
            {
                if (chances[i] >= hotPercent) [[likely]] {
                    res += cold_work(values[i]);
                } else {
                    res += values[i];
                }
            }

            bench::do_not_optimize(res);
        });

        // Reference: hot path written first, no hints (compiler guesses right).
        add_test("reference (hot path first, no hints)", [this]() {
            uint64_t res = 0;

            for (int i = 0; i < kSize; i++)
            {
                if (chances[i] < hotPercent) {
                    res += values[i];
                } else {
                    res += cold_work(values[i]);
                }
            }

            bench::do_not_optimize(res);
        });
    }
};

// Automatic self-registration of example benchmark classes
REGISTER_BENCHMARK_CLASS(LikelyUnlikelyMediumBenchmark)