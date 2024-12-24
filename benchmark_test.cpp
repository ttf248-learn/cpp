#include <benchmark/benchmark.h>

static void BM_StringCreation(benchmark::State& state) {
    for (auto _ : state) {
        std::string empty_string;
        benchmark::DoNotOptimize(empty_string);
    }
}
BENCHMARK(BM_StringCreation);

static void BM_StringCopy(benchmark::State& state) {
    std::string source = "Hello, Google Benchmark!";
    for (auto _ : state) {
        std::string copy = source;
        benchmark::DoNotOptimize(copy);
    }
}
BENCHMARK(BM_StringCopy);

BENCHMARK_MAIN();

// 下载自行编译：cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/opt/benchmark-1.8.5 -DBENCHMARK_ENABLE_TESTING=0 ..
// 系统安装一直提示版本不对，所以自行编译安装：Google Benchmark: "***WARNING*** Library was built as DEBUG. Timings may be affected."
// g++ -std=c++11 -O2 -isystem /usr/local/include -L /usr/local/lib -o benchmark_test benchmark_test.cpp -lbenchmark -pthread
// sudo apt install cmake g++ libbenchmark-dev
// cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/opt/benchmark-1.8.5 -DBENCHMARK_ENABLE_TESTING=0 ..
// g++ -std=c++11 -O2 -isystem /opt/benchmark-1.8.5/include -L /opt/benchmark-1.8.5/lib -o benchmark_test benchmark_test.cpp -lbenchmark -pthread

// 2024-11-15T10:07:22+08:00
// Running ./benchmark_test
// Run on (2 X 2500 MHz CPU s)
// CPU Caches:
//   L1 Data 32 KiB (x1)
//   L1 Instruction 32 KiB (x1)
//   L2 Unified 1024 KiB (x1)
//   L3 Unified 33792 KiB (x1)
// Load Average: 0.06, 0.32, 0.46
// ***WARNING*** Library was built as DEBUG. Timings may be affected.
// ------------------------------------------------------------
// Benchmark                  Time             CPU   Iterations
// ------------------------------------------------------------
// BM_StringCreation       1.15 ns         1.15 ns    619690212
// BM_StringCopy           20.1 ns         20.1 ns     28809938

// 2024-11-15T11:03:38+08:00
// Running ./benchmark_test
// Run on (2 X 2500 MHz CPU s)
// CPU Caches:
//   L1 Data 32 KiB (x1)
//   L1 Instruction 32 KiB (x1)
//   L2 Unified 1024 KiB (x1)
//   L3 Unified 33792 KiB (x1)
// Load Average: 0.04, 0.16, 0.09
// ------------------------------------------------------------
// Benchmark                  Time             CPU   Iterations
// ------------------------------------------------------------
// BM_StringCreation       1.03 ns         1.03 ns    703773252
// BM_StringCopy           18.2 ns         18.2 ns     37350685