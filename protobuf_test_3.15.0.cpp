#include <benchmark/benchmark.h>
#include "pb_3.15.0/message.pb.h"
#include <string>
#include <vector>

// 序列化基准测试
static void BM_PB_Serialize(benchmark::State& state) {
    benchmark::TestMessage message;
    message.set_id(1);
    message.set_name("test");
    for (int i = 0; i < 100; ++i) {
        message.add_values(i);
    }

    for (auto _ : state) {
        std::string output;
        message.SerializeToString(&output);
    }
}

BENCHMARK(BM_PB_Serialize);

// 反序列化基准测试
static void BM_PB_Deserialize(benchmark::State& state) {
    benchmark::TestMessage message;
    message.set_id(1);
    message.set_name("test");
    for (int i = 0; i < 100; ++i) {
        message.add_values(i);
    }

    std::string output;
    message.SerializeToString(&output);

    for (auto _ : state) {
        benchmark::TestMessage new_message;
        new_message.ParseFromString(output);
    }
}

BENCHMARK(BM_PB_Deserialize);