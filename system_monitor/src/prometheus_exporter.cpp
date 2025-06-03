#include "prometheus_exporter.h"
#include <iostream>

PrometheusExporter::PrometheusExporter(const std::string& bind_address)
    : exposer_(std::make_unique<prometheus::Exposer>(bind_address)) {
    registry_ = std::make_shared<prometheus::Registry>();
    exposer_->RegisterCollectable(registry_);
    InitializeMetrics();
}

PrometheusExporter::~PrometheusExporter() = default;

void PrometheusExporter::InitializeMetrics() {
    // 初始化基础性能指标
    cpu_usage_family_ = &prometheus::BuildGauge()
        .Name("process_cpu_usage_percent")
        .Help("CPU usage percentage of the process")
        .Register(*registry_);

    memory_usage_family_ = &prometheus::BuildGauge()
        .Name("process_memory_usage")
        .Help("Memory usage statistics")
        .Register(*registry_);

    thread_count_family_ = &prometheus::BuildGauge()
        .Name("process_thread_count")
        .Help("Number of threads in the process")
        .Register(*registry_);

    // 初始化系统负载指标
    load_average_family_ = &prometheus::BuildGauge()
        .Name("system_load_average")
        .Help("System load average")
        .Register(*registry_);

    process_count_family_ = &prometheus::BuildGauge()
        .Name("system_process_count")
        .Help("System process statistics")
        .Register(*registry_);

    // 初始化磁盘IO指标
    disk_io_family_ = &prometheus::BuildGauge()
        .Name("process_disk_io")
        .Help("Disk I/O statistics")
        .Register(*registry_);

    // 初始化网络指标
    network_io_family_ = &prometheus::BuildGauge()
        .Name("process_network_io")
        .Help("Network I/O statistics")
        .Register(*registry_);

    // 初始化进程状态指标
    process_status_family_ = &prometheus::BuildGauge()
        .Name("process_status")
        .Help("Process status information")
        .Register(*registry_);

    file_descriptor_family_ = &prometheus::BuildGauge()
        .Name("process_file_descriptors")
        .Help("File descriptor statistics")
        .Register(*registry_);

    context_switches_family_ = &prometheus::BuildCounter()
        .Name("process_context_switches")
        .Help("Context switch statistics")
        .Register(*registry_);

    // 初始化运行时间指标
    uptime_family_ = &prometheus::BuildCounter()
        .Name("uptime_seconds")
        .Help("System and process uptime in seconds")
        .Register(*registry_);
}

void PrometheusExporter::UpdateMetrics(const SystemInfo& info) {
    // 更新CPU使用率
    cpu_usage_family_->Add({}).Set(info.cpu_usage_percent);

    // 更新内存使用情况
    auto& memory_gauge = memory_usage_family_->Add({
        {"type", "used_mb"}
    });
    memory_gauge.Set(info.memory_used_mb);

    memory_usage_family_->Add({
        {"type", "total_mb"}
    }).Set(info.memory_total_mb);

    memory_usage_family_->Add({
        {"type", "usage_percent"}
    }).Set(info.memory_usage_percent);

    // 更新线程数
    thread_count_family_->Add({}).Set(info.thread_count);

    // 更新系统负载
    load_average_family_->Add({
        {"period", "1min"}
    }).Set(info.system_load.load_average_1min);

    load_average_family_->Add({
        {"period", "5min"}
    }).Set(info.system_load.load_average_5min);

    load_average_family_->Add({
        {"period", "15min"}
    }).Set(info.system_load.load_average_15min);

    // 更新进程数量
    process_count_family_->Add({
        {"state", "running"}
    }).Set(info.system_load.running_processes);

    process_count_family_->Add({
        {"state", "total"}
    }).Set(info.system_load.total_processes);

    // 更新磁盘IO
    disk_io_family_->Add({
        {"operation", "read"},
        {"unit", "bytes_per_sec"}
    }).Set(info.disk_io.read_bytes_per_sec);

    disk_io_family_->Add({
        {"operation", "write"},
        {"unit", "bytes_per_sec"}
    }).Set(info.disk_io.write_bytes_per_sec);

    disk_io_family_->Add({
        {"operation", "read"},
        {"unit", "ops_per_sec"}
    }).Set(info.disk_io.read_ops_per_sec);

    disk_io_family_->Add({
        {"operation", "write"},
        {"unit", "ops_per_sec"}
    }).Set(info.disk_io.write_ops_per_sec);

    // 更新网络IO
    network_io_family_->Add({
        {"direction", "receive"},
        {"unit", "bytes_per_sec"}
    }).Set(info.network.bytes_recv_per_sec);

    network_io_family_->Add({
        {"direction", "send"},
        {"unit", "bytes_per_sec"}
    }).Set(info.network.bytes_sent_per_sec);

    network_io_family_->Add({
        {"direction", "receive"},
        {"unit", "packets_per_sec"}
    }).Set(info.network.packets_recv_per_sec);

    network_io_family_->Add({
        {"direction", "send"},
        {"unit", "packets_per_sec"}
    }).Set(info.network.packets_sent_per_sec);

    // 更新进程状态
    process_status_family_->Add({
        {"type", "state"}
    }).Set(1); // 设置为1表示当前状态

    // 更新文件描述符
    file_descriptor_family_->Add({
        {"type", "open"}
    }).Set(info.process.open_files_count);

    file_descriptor_family_->Add({
        {"type", "max"}
    }).Set(info.process.max_open_files);

    file_descriptor_family_->Add({
        {"type", "usage_percent"}
    }).Set(info.process.file_descriptor_usage_percent);

    // 更新上下文切换计数
    context_switches_family_->Add({
        {"type", "voluntary"}
    }).Increment(info.process.voluntary_context_switches);

    context_switches_family_->Add({
        {"type", "involuntary"}
    }).Increment(info.process.involuntary_context_switches);

    // 更新运行时间
    uptime_family_->Add({
        {"type", "system"}
    }).Increment(info.system_uptime_seconds);

    uptime_family_->Add({
        {"type", "process"}
    }).Increment(info.process_uptime_seconds);
}