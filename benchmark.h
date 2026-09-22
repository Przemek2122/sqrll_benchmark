#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <algorithm>
#include <numeric>
#include <iomanip>
#include <cstdint>
#include <cmath>
#include <thread>
#include <atomic>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <x86intrin.h>
#endif

#if defined(__linux__)
#include <sched.h>
#include <pthread.h>
#include <unistd.h>
#elif defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__FreeBSD__) || defined(__DragonFly__)
#include <pthread_np.h>
#include <sys/cpuset.h>
#include <unistd.h>
#endif

namespace bench {

// Prevents compiler optimizations from eliminating evaluated expressions or dead code
template <typename T>
inline __attribute__((always_inline)) void do_not_optimize(T const& value) {
#if defined(__GNUC__) || defined(__clang__)
    asm volatile("" : : "g"(value) : "memory");
#else
    std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
}

template <typename T>
inline __attribute__((always_inline)) void do_not_optimize(T& value) {
#if defined(__clang__)
    asm volatile("" : "+r,m"(value) : : "memory");
#elif defined(__GNUC__)
    asm volatile("" : "+m,r"(value) : : "memory");
#else
    std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
}

// Memory barrier to force memory loads/stores synchronization
inline void clobber_memory() {
#if defined(__GNUC__) || defined(__clang__)
    asm volatile("" : : : "memory");
#else
    std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
}

// High-precision CPU cycle counter using serialized instruction stream
class CycleTimer {
public:
    static inline uint64_t get_cycles_start() {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
        _mm_lfence();
        const uint64_t cycles = __rdtsc();
        _mm_lfence();
        return cycles;
#else
        // Fallback for non-x86 architectures
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
        return static_cast<uint64_t>(ts.tv_sec) * 1000000000ULL + ts.tv_nsec;
#endif
    }

    static inline uint64_t get_cycles_end() {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
        unsigned int aux;
        uint64_t cycles = __rdtscp(&aux);
        _mm_lfence();
        return cycles;
#else
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
        return static_cast<uint64_t>(ts.tv_sec) * 1000000000ULL + ts.tv_nsec;
#endif
    }

    // Measures the timer call overhead in CPU cycles for calibration
    static uint64_t measure_overhead(size_t iterations = 10000) {
        uint64_t min_overhead = UINT64_MAX;
        for (size_t i = 0; i < iterations; ++i) {
            const uint64_t start = get_cycles_start();
            const uint64_t end = get_cycles_end();
            const uint64_t diff = (end >= start) ? (end - start) : 0;
            if (diff < min_overhead) {
                min_overhead = diff;
            }
        }
        return min_overhead;
    }
};

// Lightweight spin barrier for high-precision thread synchronization during cycle measurements
class SpinBarrier {
public:
    explicit SpinBarrier(size_t count)
        : threshold_(count), count_(count), generation_(0) {}

    void wait() {
        if (threshold_ <= 1) {
            return;
        }
        size_t gen = generation_.load(std::memory_order_acquire);
        if (count_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            count_.store(threshold_, std::memory_order_release);
            generation_.fetch_add(1, std::memory_order_release);
        } else {
            while (generation_.load(std::memory_order_acquire) == gen) {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
                _mm_pause();
#else
                std::this_thread::yield();
#endif
            }
        }
    }

private:
    const size_t threshold_;
    std::atomic<size_t> count_;
    std::atomic<size_t> generation_;
};

// Returns the list of available CPU core IDs for the current process
inline std::vector<unsigned int> get_available_cpus() {
    std::vector<unsigned int> cpus;
#if defined(__linux__)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    if (sched_getaffinity(0, sizeof(cpu_set_t), &cpuset) == 0) {
        for (int i = 0; i < CPU_SETSIZE; ++i) {
            if (CPU_ISSET(i, &cpuset)) {
                cpus.push_back(static_cast<unsigned int>(i));
            }
        }
    }
#elif defined(_WIN32) || defined(_WIN64)
    DWORD_PTR process_affinity_mask = 0;
    DWORD_PTR system_affinity_mask = 0;
    if (GetProcessAffinityMask(GetCurrentProcess(), &process_affinity_mask, &system_affinity_mask)) {
        for (unsigned int i = 0; i < sizeof(DWORD_PTR) * 8; ++i) {
            if ((process_affinity_mask >> i) & 1) {
                cpus.push_back(i);
            }
        }
    }
#elif defined(__FreeBSD__) || defined(__DragonFly__)
    cpuset_t mask;
    if (cpuset_getaffinity(CPU_LEVEL_WHICH, CPU_WHICH_PID, -1, sizeof(mask), &mask) == 0) {
        for (int i = 0; i < CPU_SETSIZE; ++i) {
            if (CPU_ISSET(i, &mask)) {
                cpus.push_back(static_cast<unsigned int>(i));
            }
        }
    }
#endif

    if (cpus.empty()) {
        unsigned int count = std::thread::hardware_concurrency();
        if (count == 0) {
            count = 1;
        }
        for (unsigned int i = 0; i < count; ++i) {
            cpus.push_back(i);
        }
    }
    return cpus;
}

// Returns CPU core IDs available for pinning worker threads (excluding core 0 which is kept for OS)
inline std::vector<unsigned int> get_pinning_cpus(const std::vector<unsigned int>& available_cpus) {
    std::vector<unsigned int> pinning_cpus;
    for (unsigned int cpu : available_cpus) {
        if (cpu != 0) {
            pinning_cpus.push_back(cpu);
        }
    }
    return pinning_cpus;
}

// Safely pins the current calling thread to a specific CPU core
inline bool pin_current_thread_to_cpu(unsigned int cpu_id) {
#if defined(__linux__)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(static_cast<int>(cpu_id), &cpuset);
    return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0;
#elif defined(_WIN32) || defined(_WIN64)
    if (cpu_id < sizeof(DWORD_PTR) * 8) {
        DWORD_PTR mask = static_cast<DWORD_PTR>(1) << cpu_id;
        return SetThreadAffinityMask(GetCurrentThread(), mask) != 0;
    }
    return false;
#elif defined(__FreeBSD__) || defined(__DragonFly__)
    cpuset_t mask;
    CPU_ZERO(&mask);
    CPU_SET(static_cast<int>(cpu_id), &mask);
    return cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1, sizeof(mask), &mask) == 0;
#else
    (void)cpu_id;
    return false;
#endif
}

// Result metrics of a benchmarked test case
struct BenchmarkResult {
    std::string test_name;
    size_t num_threads = 1;
    uint64_t min_cycles = 0;
    uint64_t max_cycles = 0;
    double median_cycles = 0.0;
    double avg_cycles = 0.0;
    double std_dev = 0.0;
    size_t iterations = 0;
};

// Single callable test case definition supporting single-threaded and multi-threaded workloads
struct BenchmarkTestCase {
    std::string name;
    std::function<void(size_t thread_id)> func;
    size_t num_threads = 1;
};

// Base class for any custom benchmark class
class BenchmarkGroup {
public:
    explicit BenchmarkGroup(std::string name) : group_name_(std::move(name)) {}
    virtual ~BenchmarkGroup() = default;

    // Optional lifecycle hooks called before and after executing the group's tests
    virtual void set_up() {}
    virtual void tear_down() {}

    // Register single-threaded benchmark function
    void add_test(const std::string& test_name, std::function<void()> test_func) {
        tests_.push_back({test_name, [f = std::move(test_func)](size_t) { f(); }, 1});
    }

    // Register multithreaded benchmark function (with thread_id)
    void add_test(const std::string& test_name, size_t num_threads, std::function<void(size_t thread_id)> test_func) {
        tests_.push_back({test_name, std::move(test_func), std::max<size_t>(1, num_threads)});
    }

    // Register multithreaded benchmark function (without thread_id)
    void add_test(const std::string& test_name, size_t num_threads, std::function<void()> test_func) {
        tests_.push_back({test_name, [f = std::move(test_func)](size_t) { f(); }, std::max<size_t>(1, num_threads)});
    }

    // Explicit helper for multithreaded test registration
    void add_multithreaded_test(const std::string& test_name, size_t num_threads, std::function<void(size_t thread_id)> test_func) {
        add_test(test_name, num_threads, std::move(test_func));
    }

    void add_multithreaded_test(const std::string& test_name, size_t num_threads, std::function<void()> test_func) {
        add_test(test_name, num_threads, std::move(test_func));
    }

    [[nodiscard]] const std::string& get_name() const { return group_name_; }
    [[nodiscard]] const std::vector<BenchmarkTestCase>& get_tests() const { return tests_; }

private:
    std::string group_name_;
    std::vector<BenchmarkTestCase> tests_;
};

// Runner execution configuration
struct BenchmarkConfig {
    size_t warmup_iterations = 200;      // Cache and branch predictor warmup
    size_t measurement_iterations = 1000; // Sample count for deterministic statistics
    bool subtract_overhead = true;        // Deduct timer instruction overhead
    bool pin_threads = true;              // Pin worker threads to dedicated CPU cores (excluding core 0)
};

// Central runner managing benchmark classes and test executions
class BenchmarkRunner {
public:
    static BenchmarkRunner& instance() {
        static BenchmarkRunner runner;
        return runner;
    }

    // Register an instance of a benchmark group
    void register_group(std::shared_ptr<BenchmarkGroup> group) {
        groups_.push_back(std::move(group));
    }

    // Template helper to create and register benchmark classes
    template <typename T, typename... Args>
    std::shared_ptr<T> add_benchmark_class(Args&&... args) {
        auto group = std::make_shared<T>(std::forward<Args>(args)...);
        register_group(group);
        return group;
    }

    // Executes all registered benchmark classes and their tests
    void start(const BenchmarkConfig& config = {}) {
        std::cout << "\n==================================================================================================\n";
        std::cout << "                             CPU CYCLE BENCHMARK FRAMEWORK                                        \n";
        std::cout << "==================================================================================================\n";

        uint64_t timer_overhead = config.subtract_overhead ? CycleTimer::measure_overhead() : 0;
        std::vector<unsigned int> available_cpus = get_available_cpus();

        std::cout << "[INFO] Registered Benchmark Classes: " << groups_.size() << "\n";
        std::cout << "[INFO] Warmup Iterations:            " << config.warmup_iterations << "\n";
        std::cout << "[INFO] Measurement Iterations:       " << config.measurement_iterations << "\n";
        std::cout << "[INFO] Calibrated Timer Overhead:    " << timer_overhead << " cycles\n";
        std::cout << "--------------------------------------------------------------------------------------------------\n";

        for (const auto& group : groups_) {
            std::cout << "\n>>> Class: [" << group->get_name() << "] (" 
                      << group->get_tests().size() << " test functions)\n";

            std::cout << std::left 
                      << std::setw(56) << "  Test Function"
                      << std::right 
                      << std::setw(10) << "Threads"
                      << std::setw(14) << "Min (Cycles)"
                      << std::setw(14) << "Median"
                      << std::setw(14) << "Average"
                      << std::setw(14) << "Std Dev"
                      << "\n";
            std::cout << "  " << std::string(122, '-') << "\n";

            group->set_up();

            for (const auto& test : group->get_tests()) {
                BenchmarkResult res = run_single_test(test, config, timer_overhead, available_cpus);
                print_result(res);
            }

            group->tear_down();
        }

        std::cout << "\n==================================================================================================\n";
        std::cout << "                                  BENCHMARK COMPLETED                                             \n";
        std::cout << "==================================================================================================\n\n";
    }

private:
    BenchmarkRunner() = default;

    BenchmarkResult run_single_test(const BenchmarkTestCase& test, const BenchmarkConfig& config, uint64_t timer_overhead, const std::vector<unsigned int>& available_cpus) {
        if (test.num_threads <= 1) {
            return run_single_threaded(test, config, timer_overhead);
        } else {
            return run_multi_threaded(test, config, timer_overhead, available_cpus);
        }
    }

    BenchmarkResult run_single_threaded(const BenchmarkTestCase& test, const BenchmarkConfig& config, uint64_t timer_overhead) {
        // Warmup phase to populate CPU caches and train branch predictors
        for (size_t i = 0; i < config.warmup_iterations; ++i) {
            test.func(0);
        }

        std::vector<uint64_t> samples;
        samples.reserve(config.measurement_iterations);

        // Core measurement loop
        for (size_t i = 0; i < config.measurement_iterations; ++i) {
            clobber_memory();
            uint64_t start = CycleTimer::get_cycles_start();
            test.func(0);
            uint64_t end = CycleTimer::get_cycles_end();
            clobber_memory();

            uint64_t elapsed = (end >= start) ? (end - start) : 0;
            if (config.subtract_overhead && elapsed >= timer_overhead) {
                elapsed -= timer_overhead;
            } else if (config.subtract_overhead) {
                elapsed = 0;
            }
            samples.push_back(elapsed);
        }

        return calculate_result(test.name, 1, samples);
    }

    BenchmarkResult run_multi_threaded(const BenchmarkTestCase& test, const BenchmarkConfig& config, uint64_t timer_overhead, const std::vector<unsigned int>& available_cpus) {
        std::vector<unsigned int> pinning_cpus;
        if (config.pin_threads) {
            pinning_cpus = get_pinning_cpus(available_cpus);
        }

        const size_t requested_workers = test.num_threads > 1 ? test.num_threads - 1 : 0;
        size_t actual_workers = requested_workers;

        if (config.pin_threads) {
            const size_t max_workers = pinning_cpus.size();
            if (requested_workers > max_workers) {
                const size_t abandoned = requested_workers - max_workers;
                std::cout << "[WARNING]: " << abandoned << " threads abandoned due to impossiblity to pin for deterministic output.\n";
                actual_workers = max_workers;
            }
        }

        const size_t actual_threads = 1 + actual_workers;
        if (actual_threads <= 1) {
            return run_single_threaded(test, config, timer_overhead);
        }

        const size_t total_iterations = config.warmup_iterations + config.measurement_iterations;

        SpinBarrier start_barrier(actual_threads);
        SpinBarrier end_barrier(actual_threads);

        std::vector<uint64_t> samples;
        samples.reserve(config.measurement_iterations);

        std::vector<std::thread> workers;
        workers.reserve(actual_workers);

        auto worker_loop = [&](size_t thread_id, unsigned int cpu_id) {
            if (config.pin_threads) {
                pin_current_thread_to_cpu(cpu_id);
            }
            for (size_t iter = 0; iter < total_iterations; ++iter) {
                start_barrier.wait();
                test.func(thread_id);
                end_barrier.wait();
            }
        };

        for (size_t t = 1; t <= actual_workers; ++t) {
            unsigned int target_cpu = config.pin_threads ? pinning_cpus[t - 1] : 0;
            workers.emplace_back(worker_loop, t, target_cpu);
        }

        // Main thread executes as thread_id = 0 (unpinned, core 0 reserved for OS)
        for (size_t iter = 0; iter < total_iterations; ++iter) {
            bool is_measurement = (iter >= config.warmup_iterations);

            clobber_memory();
            start_barrier.wait();
            uint64_t start = CycleTimer::get_cycles_start();

            test.func(0);

            end_barrier.wait();
            uint64_t end = CycleTimer::get_cycles_end();
            clobber_memory();

            if (is_measurement) {
                uint64_t elapsed = (end >= start) ? (end - start) : 0;
                if (config.subtract_overhead && elapsed >= timer_overhead) {
                    elapsed -= timer_overhead;
                } else if (config.subtract_overhead) {
                    elapsed = 0;
                }
                samples.push_back(elapsed);
            }
        }

        for (auto& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }

        return calculate_result(test.name, actual_threads, samples);
    }

    BenchmarkResult calculate_result(const std::string& name, size_t num_threads, std::vector<uint64_t>& samples) {
        if (samples.empty()) {
            return BenchmarkResult{.test_name = name, .num_threads = num_threads};
        }

        // Statistical evaluation (min provides the most deterministic cycle count)
        std::ranges::sort(samples);

        const uint64_t min_c = samples.front();
        const uint64_t max_c = samples.back();

        double median_c = 0.0;
        if (samples.size() % 2 == 0) {
            median_c = (samples[samples.size() / 2 - 1] + samples[samples.size() / 2]) / 2.0;
        } else {
            median_c = samples[samples.size() / 2];
        }

        double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
        double avg_c = sum / samples.size();

        double sq_sum = 0.0;
        for (uint64_t val : samples) {
            double diff = static_cast<double>(val) - avg_c;
            sq_sum += diff * diff;
        }
        double std_dev = std::sqrt(sq_sum / samples.size());

        return BenchmarkResult{
            .test_name = name,
            .num_threads = num_threads,
            .min_cycles = min_c,
            .max_cycles = max_c,
            .median_cycles = median_c,
            .avg_cycles = avg_c,
            .std_dev = std_dev,
            .iterations = samples.size()
        };
    }

    static void print_result(const BenchmarkResult& res) {
        const std::string display_name = "  " + res.test_name;
        std::cout << std::left 
                  << std::setw(56) << display_name
                  << std::right 
                  << std::setw(10) << res.num_threads
                  << std::setw(14) << res.min_cycles
                  << std::setw(14) << std::fixed << std::setprecision(1) << res.median_cycles
                  << std::setw(14) << std::fixed << std::setprecision(1) << res.avg_cycles
                  << std::setw(14) << std::fixed << std::setprecision(2) << res.std_dev
                  << "\n";
    }

    std::vector<std::shared_ptr<BenchmarkGroup>> groups_;
};

// Macro for static/automatic registration of benchmark classes
#define REGISTER_BENCHMARK_CLASS(ClassName) \
    static inline const bool ClassName##_registered = []() { \
        ::bench::BenchmarkRunner::instance().add_benchmark_class<ClassName>(); \
        return true; \
    }();

} // namespace bench
