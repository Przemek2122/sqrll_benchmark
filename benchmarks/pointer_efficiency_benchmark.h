#pragma once

#include <cstdint>
#include <random>
#include <vector>

#include "../benchmark.h"

constexpr uint64_t DataTestSize = 20000;
constexpr uint64_t DataTestSizeMinusOne = DataTestSize - 1;

struct TestData
{
    std::vector<uint32_t> Data;
};

class TestB
{
public:
    TestData* Data;
};

class TestA
{
public:
    TestB* B;
};

class PointerEfficiencyBenchmark : public bench::BenchmarkGroup {
private:
    std::vector<std::unique_ptr<TestA>> TestVecA;
    std::vector<std::unique_ptr<TestB>> TestVecB;
    std::vector<std::unique_ptr<TestData>> TestVecDataPtr;
    std::vector<TestData> TestVecDataNoPtr;

public:
    void set_up() override
    {
        TestVecA.reserve(DataTestSize);
        TestVecB.reserve(DataTestSize);
        TestVecDataPtr.reserve(DataTestSize);
        TestVecDataNoPtr.reserve(DataTestSize);

        std::mt19937_64 gen{12345};
        std::uniform_int_distribution<int>      chanceDist(0, UINT32_MAX);
        std::uniform_int_distribution<uint64_t> valueDist;

        // Feed some random data idk
        for (int i = 0; i < DataTestSize; i++)
        {
            TestData testData;

            testData.Data.reserve(DataTestSize);

            for (int j = 0; j < DataTestSize; j++)
            {
                testData.Data.push_back(chanceDist(gen));
            }

            TestVecDataNoPtr.push_back(testData);
        }

        // Use random data we already have access to
        for (int i = 0; i < DataTestSize; i++)
        {
            TestVecDataPtr.push_back(std::make_unique<TestData>(TestVecDataNoPtr[i]));
        }

        for (int i = 0; i < DataTestSize; i++)
        {
            TestB testB {};
            testB.Data = TestVecDataPtr[i].get();

            TestVecB.push_back(std::make_unique<TestB>(testB));
        }

        for (int i = 0; i < DataTestSize; i++)
        {
            TestA testA {};
            testA.B = TestVecB[i].get();

            TestVecA.push_back(std::make_unique<TestA>(testA));
        }
    }

    PointerEfficiencyBenchmark() : bench::BenchmarkGroup(std::string("Performance of pointer acces on big dataset : '") + std::to_string(DataTestSize) + "'.")
    {
        add_test("Test no pointer access", [this]() {
            uint64_t res = 0;

            for (int i = 0; i < DataTestSize; i++)
            {
                res += TestVecDataNoPtr[DataTestSizeMinusOne-i].Data[i] + TestVecDataNoPtr[i].Data[DataTestSizeMinusOne-i];
            }

            bench::do_not_optimize(res);
        });

        add_test("Test 1 pointer access", [this]() {
            uint64_t res = 0;

            for (int i = 0; i < DataTestSize; i++)
            {
                res += TestVecDataPtr[DataTestSizeMinusOne-i]->Data[i] + TestVecDataPtr[i]->Data[DataTestSizeMinusOne-i];
            }

            bench::do_not_optimize(res);
        });

        add_test("Test 2 pointers access", [this]() {
            uint64_t res = 0;

            for (int i = 0; i < DataTestSize; i++)
            {
                res += TestVecB[DataTestSizeMinusOne-i]->Data->Data[i] + TestVecB[i]->Data->Data[DataTestSizeMinusOne-i];
            }

            bench::do_not_optimize(res);
        });

        add_test("Test 3 pointers access", [this]() {
            uint64_t res = 0;

            for (int i = 0; i < DataTestSize; i++)
            {
                res += TestVecA[DataTestSizeMinusOne-i]->B->Data->Data[i] + TestVecA[i]->B->Data->Data[DataTestSizeMinusOne-i];
            }

            bench::do_not_optimize(res);
        });
    }
};

// Automatic self-registration
REGISTER_BENCHMARK_CLASS(PointerEfficiencyBenchmark)
