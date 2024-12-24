// pc_quota_client_mng_benchmark.cpp
#include <benchmark/benchmark.h>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include "thread_safe_map.h"

using namespace std;

std::mutex mtx;
std::map<string, string> normal_map;

static void BM_ThreadSafeMap_Insert(benchmark::State &state)
{
    ThreadSafeMap<string, string> thread_safe_map;
    for (auto _ : state)
    {
        thread_safe_map.insert("key", "value");
    }
}
BENCHMARK(BM_ThreadSafeMap_Insert);

static void BM_NormalMap_Insert(benchmark::State &state)
{
    for (auto _ : state)
    {
        std::lock_guard<std::mutex> lock(mtx);
        normal_map["key"] = "value";
    }
}
BENCHMARK(BM_NormalMap_Insert);

static void BM_ThreadSafeMap_Get(benchmark::State &state)
{
    ThreadSafeMap<string, string> thread_safe_map;
    thread_safe_map.insert("key", "value");
    string value;
    for (auto _ : state)
    {
        thread_safe_map.get("key", value);
    }
}
BENCHMARK(BM_ThreadSafeMap_Get);

static void BM_NormalMap_Get(benchmark::State &state)
{
    normal_map["key"] = "value";
    string value;
    for (auto _ : state)
    {
        std::lock_guard<std::mutex> lock(mtx);
        value = normal_map["key"];
    }
}
BENCHMARK(BM_NormalMap_Get);

static void BM_ThreadSafeMap_Erase(benchmark::State &state)
{
    ThreadSafeMap<string, string> thread_safe_map;
    thread_safe_map.insert("key", "value");
    for (auto _ : state)
    {
        thread_safe_map.erase("key");
        thread_safe_map.insert("key", "value");
    }
}
BENCHMARK(BM_ThreadSafeMap_Erase);

static void BM_NormalMap_Erase(benchmark::State &state)
{
    for (auto _ : state)
    {
        std::lock_guard<std::mutex> lock(mtx);
        normal_map.erase("key");
        normal_map["key"] = "value";
    }
}
BENCHMARK(BM_NormalMap_Erase);

// BENCHMARK_MAIN();
