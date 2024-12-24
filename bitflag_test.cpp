#include <benchmark/benchmark.h>
#include <cstdint>
#include <array>

// 位标志方法：使用 uint32_t 来存储 8 个标志
class BitFlags {
public:
    uint32_t flags = 0;

    // 设置标志
    void setFlag(int flag) {
        flags |= (1 << flag);
    }

    // 清除标志
    void clearFlag(int flag) {
        flags &= ~(1 << flag);
    }

    // 检查标志
    bool isFlagSet(int flag) const {
        return flags & (1 << flag);
    }
};

// 不同 `int` 数值标志方法：使用不同的整数值表示标志
class IntValueFlags {
public:
    static const int RiskWarning = 1;         // 标志 1
    static const int Delisted = 2;            // 标志 2
    static const int Transferred = 4;         // 标志 3
    static const int NotProfitable = 8;       // 标志 4
    static const int DualClassShares = 16;    // 标志 5
    static const int CDR = 32;                // 标志 6
    static const int Registered = 64;         // 标志 7
    static const int DelistedFinal = 128;     // 标志 8

    int flags = 0;

    // 设置标志
    void setFlag(int flag) {
        flags |= flag;
    }

    // 清除标志
    void clearFlag(int flag) {
        flags &= ~flag;
    }

    // 检查标志
    bool isFlagSet(int flag) const {
        return flags & flag;
    }
};

// 基准测试：使用位标志设置标志
static void BM_BitFlags_SetFlag(benchmark::State& state) {
    BitFlags bitFlags;
    for (auto _ : state) {
        bitFlags.setFlag(3);  // 设置第 3 位标志
        benchmark::DoNotOptimize(bitFlags.flags);  // 防止优化
    }
}
BENCHMARK(BM_BitFlags_SetFlag);

// 基准测试：使用不同 `int` 数值标志设置标志
static void BM_IntValueFlags_SetFlag(benchmark::State& state) {
    IntValueFlags intFlags;
    for (auto _ : state) {
        intFlags.setFlag(IntValueFlags::Transferred);  // 设置 Transferred 标志
        benchmark::DoNotOptimize(intFlags.flags);  // 防止优化
    }
}
BENCHMARK(BM_IntValueFlags_SetFlag);

// 基准测试：使用位标志检查标志
static void BM_BitFlags_CheckFlag(benchmark::State& state) {
    BitFlags bitFlags;
    bitFlags.setFlag(3);  // 预先设置第 3 位标志
    for (auto _ : state) {
        bitFlags.isFlagSet(3);  // 检查第 3 位标志
        benchmark::DoNotOptimize(bitFlags.flags);  // 防止优化
    }
}
BENCHMARK(BM_BitFlags_CheckFlag);

// 基准测试：使用不同 `int` 数值标志检查标志
static void BM_IntValueFlags_CheckFlag(benchmark::State& state) {
    IntValueFlags intFlags;
    intFlags.setFlag(IntValueFlags::Transferred);  // 预先设置 Transferred 标志
    for (auto _ : state) {
        intFlags.isFlagSet(IntValueFlags::Transferred);  // 检查 Transferred 标志
        benchmark::DoNotOptimize(intFlags.flags);  // 防止优化
    }
}
BENCHMARK(BM_IntValueFlags_CheckFlag);

// 基准测试：使用位标志清除标志
static void BM_BitFlags_ClearFlag(benchmark::State& state) {
    BitFlags bitFlags;
    bitFlags.setFlag(3);  // 预先设置第 3 位标志
    for (auto _ : state) {
        bitFlags.clearFlag(3);  // 清除第 3 位标志
        benchmark::DoNotOptimize(bitFlags.flags);  // 防止优化
    }
}
BENCHMARK(BM_BitFlags_ClearFlag);

// 基准测试：使用不同 `int` 数值标志清除标志
static void BM_IntValueFlags_ClearFlag(benchmark::State& state) {
    IntValueFlags intFlags;
    intFlags.setFlag(IntValueFlags::Transferred);  // 预先设置 Transferred 标志
    for (auto _ : state) {
        intFlags.clearFlag(IntValueFlags::Transferred);  // 清除 Transferred 标志
        benchmark::DoNotOptimize(intFlags.flags);  // 防止优化
    }
}

BENCHMARK(BM_IntValueFlags_ClearFlag);
