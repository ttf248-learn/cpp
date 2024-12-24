#include <benchmark/benchmark.h>
#include <unordered_map>
#include <random>
#include "market_inst.h"

struct StockDict
{
    // 假设 StockDict 有一些数据成员
    int data;
};

static void BM_MarketInst_Create(benchmark::State &state)
{
    for (auto _ : state)
    {
        MarketInst mi("market", "inst", 1);
        benchmark::DoNotOptimize(mi);
    }
}
BENCHMARK(BM_MarketInst_Create);

static void BM_MarketInst_Copy(benchmark::State &state)
{
    MarketInst mi("market", "inst", 1);
    for (auto _ : state)
    {
        MarketInst mi_copy(mi);
        benchmark::DoNotOptimize(mi_copy);
    }
}
BENCHMARK(BM_MarketInst_Copy);

static void BM_MarketInst_Compare(benchmark::State &state)
{
    MarketInst mi1("market", "inst", 1);
    MarketInst mi2("market", "inst", 1);
    for (auto _ : state)
    {
        bool result = (mi1 == mi2);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_MarketInst_Compare);

static void BM_MarketInst_Hash(benchmark::State &state)
{
    MarketInst mi("market", "inst", 1);
    HashMarketInst hash_fn;
    for (auto _ : state)
    {
        std::size_t hash = hash_fn(mi);
        benchmark::DoNotOptimize(hash);
    }
}
BENCHMARK(BM_MarketInst_Hash);

static void BM_UnorderedMap_Find(benchmark::State &state)
{
    std::unordered_map<MarketInst, StockDict, HashMarketInst> stock_dict_map_;
    int num_elements = state.range(0);

    // 插入 num_elements 个元素
    for (int i = 0; i < num_elements; ++i)
    {
        stock_dict_map_.emplace(MarketInst("market" + std::to_string(i), "inst" + std::to_string(i), i % 2), StockDict{i});
    }

    std::mt19937 rng;
    rng.seed(std::random_device()());
    std::uniform_int_distribution<std::mt19937::result_type> dist(0, num_elements - 1);

    for (auto _ : state)
    {
        int random_index = dist(rng);
        MarketInst key_to_find("market" + std::to_string(random_index), "inst" + std::to_string(random_index), random_index % 2);
        auto it = stock_dict_map_.find(key_to_find);
        benchmark::DoNotOptimize(it);
    }
}

// 注册基准测试，测试不同数据量级别
BENCHMARK(BM_UnorderedMap_Find)->RangeMultiplier(10)->Range(10, 100000);

