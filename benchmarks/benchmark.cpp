#include "memory_pool/MemoryPool.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <new>
#include <vector>

// --------------------------------------------------
// Benchmark Object
// --------------------------------------------------
// Dono allocation methods mein same object use hoga.
// Isse comparison fair rahega.

struct BenchmarkObject
{
    std::uint64_t value;
    std::uint64_t data[3];

    explicit BenchmarkObject(std::uint64_t v)
        : value(v), data{v, v + 1, v + 2}
    {
    }

    ~BenchmarkObject() = default;
};

// Compiler ko result completely ignore karne se rokne ke liye.
volatile std::uint64_t benchmarkSink = 0;

using Clock = std::chrono::steady_clock;

// --------------------------------------------------
// Standard new/delete benchmark
// --------------------------------------------------

double benchmarkNewDelete(std::size_t iterations)
{
    // Vector allocation timing ke bahar hogi.
    // Isliye vector reallocation benchmark ko affect nahi karegi.
    std::vector<BenchmarkObject*> objects;
    objects.reserve(iterations);

    std::uint64_t checksum = 0;

    auto start = Clock::now();

    // Step 1: Allocate and construct objects.
    for (std::size_t i = 0; i < iterations; ++i)
    {
        objects.push_back(
            new BenchmarkObject(i)
        );
    }

    // Step 2: Read data, destroy and deallocate.
    for (BenchmarkObject* object : objects)
    {
        checksum += object->value;
        delete object;
    }

    auto end = Clock::now();

    benchmarkSink = benchmarkSink + checksum;

    return std::chrono::duration<double, std::milli>(
        end - start
    ).count();
}

// --------------------------------------------------
// Custom MemoryPool benchmark
// --------------------------------------------------

double benchmarkMemoryPool(std::size_t iterations)
{
    // Pool creation benchmark timing ke bahar hai.
    // Hum allocation/deallocation performance compare karenge.
    MemoryPool pool(
        sizeof(BenchmarkObject),
        iterations
    );

    std::vector<BenchmarkObject*> objects;
    objects.reserve(iterations);

    std::uint64_t checksum = 0;

    auto start = Clock::now();

    // Step 1: Allocate pool blocks and construct objects.
    for (std::size_t i = 0; i < iterations; ++i)
    {
        void* memory = pool.allocate();

        BenchmarkObject* object =
            ::new (memory) BenchmarkObject(i);

        objects.push_back(object);
    }

    // Step 2: Read data, destroy objects and return blocks.
    for (BenchmarkObject* object : objects)
    {
        checksum += object->value;

        // Placement-new object ko manually destroy karna hai.
        object->~BenchmarkObject();

        // Ab memory block ko pool mein return kar sakte hain.
        pool.deallocate(object);
    }

    auto end = Clock::now();

    benchmarkSink = benchmarkSink + checksum;

    return std::chrono::duration<double, std::milli>(
        end - start
    ).count();
}

// --------------------------------------------------
// Main benchmark runner
// --------------------------------------------------

int main()
{
    constexpr std::size_t ITERATIONS = 200000;
    constexpr int ROUNDS = 5;

    std::vector<double> newDeleteTimes;
    std::vector<double> memoryPoolTimes;

    newDeleteTimes.reserve(ROUNDS);
    memoryPoolTimes.reserve(ROUNDS);

    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "       MEMORY POOL BENCHMARK\n";
    std::cout << "========================================\n";

    std::cout << "Objects per round : "
              << ITERATIONS << '\n';

    std::cout << "Benchmark rounds  : "
              << ROUNDS << '\n';

    std::cout << "Object size       : "
              << sizeof(BenchmarkObject)
              << " bytes\n";

    // Warm-up: initial runtime effects ko reduce karne ke liye.
    benchmarkNewDelete(1000);
    benchmarkMemoryPool(1000);

    // Multiple rounds run karenge.
    for (int round = 1; round <= ROUNDS; ++round)
    {
        std::cout << "\nRound " << round << " running...\n";

        double newDeleteTime =
            benchmarkNewDelete(ITERATIONS);

        double memoryPoolTime =
            benchmarkMemoryPool(ITERATIONS);

        newDeleteTimes.push_back(newDeleteTime);
        memoryPoolTimes.push_back(memoryPoolTime);

        std::cout << "new/delete : "
                  << newDeleteTime << " ms\n";

        std::cout << "MemoryPool : "
                  << memoryPoolTime << " ms\n";
    }

    // Median calculate karenge, taaki unusual timing
    // spikes ka effect average se kam ho.
    std::sort(
        newDeleteTimes.begin(),
        newDeleteTimes.end()
    );

    std::sort(
        memoryPoolTimes.begin(),
        memoryPoolTimes.end()
    );

    double newDeleteMedian =
        newDeleteTimes[ROUNDS / 2];

    double memoryPoolMedian =
        memoryPoolTimes[ROUNDS / 2];

    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "          BENCHMARK RESULTS\n";
    std::cout << "========================================\n";

    std::cout << "new/delete median : "
              << newDeleteMedian << " ms\n";

    std::cout << "MemoryPool median : "
              << memoryPoolMedian << " ms\n";

    if (memoryPoolMedian < newDeleteMedian)
    {
        double speedup =
            newDeleteMedian / memoryPoolMedian;

        std::cout << "\nMemoryPool was faster in this run.\n";
        std::cout << "Speedup : "
                  << speedup << "x\n";
    }
    else if (memoryPoolMedian > newDeleteMedian)
    {
        double ratio =
            memoryPoolMedian / newDeleteMedian;

        std::cout << "\nnew/delete was faster in this run.\n";
        std::cout << "Ratio : "
                  << ratio << "x\n";
    }
    else
    {
        std::cout << "\nBoth methods had equal median time.\n";
    }

    std::cout << "========================================\n";

    return 0;
}