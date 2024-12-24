## pb3.6.1 benchmark

2024-12-24T13:46:16+08:00
Running /home/core/dev/benchmark_demo/build/benchmark_demo
Run on (6 X 3192.01 MHz CPU s)
CPU Caches:
  L1 Data 32 KiB (x6)
  L1 Instruction 32 KiB (x6)
  L2 Unified 256 KiB (x6)
  L3 Unified 12288 KiB (x1)
Load Average: 0.39, 0.16, 0.08
------------------------------------------------------------
Benchmark                  Time             CPU   Iterations
------------------------------------------------------------
BM_PB_Serialize         1581 ns         1580 ns       420492
BM_PB_Deserialize        962 ns          961 ns       724950

## pb29.2 benchmark

编译步骤已经调整，直接下载源码编译不太方便，谷歌已经开始启用bazel编译，git 更新到指定的分支，更新 submodules

cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/opt/protobuf-29.2 -DCMAKE_C_FLAGS="-fPIC" -DCMAKE_CXX_FLAGS="-fPIC" -Dprotobuf_BUILD_TESTS=OFF ..

能编译处理，cmake 连接的时候有问题，很多 符号找不到

