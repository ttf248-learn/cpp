#pragma once

#include <memory>
#include <string>
#include "system_monitor.h"
#include <prometheus/counter.h>
#include <prometheus/gauge.h>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>

class PrometheusExporter {
public:
    PrometheusExporter(const std::string& bind_address);
    ~PrometheusExporter();

    // 更新系统监控指标
    void UpdateMetrics(const SystemInfo& info);

private:
    // 初始化所有指标
    void InitializeMetrics();

    std::unique_ptr<prometheus::Exposer> exposer_;
    std::shared_ptr<prometheus::Registry> registry_;

    // 基础性能指标
    prometheus::Family<prometheus::Gauge>* cpu_usage_family_;
    prometheus::Family<prometheus::Gauge>* memory_usage_family_;
    prometheus::Family<prometheus::Gauge>* thread_count_family_;

    // 系统负载指标
    prometheus::Family<prometheus::Gauge>* load_average_family_;
    prometheus::Family<prometheus::Gauge>* process_count_family_;

    // 磁盘IO指标
    prometheus::Family<prometheus::Gauge>* disk_io_family_;

    // 网络指标
    prometheus::Family<prometheus::Gauge>* network_io_family_;

    // 进程状态指标
    prometheus::Family<prometheus::Gauge>* process_status_family_;
    prometheus::Family<prometheus::Gauge>* file_descriptor_family_;
    prometheus::Family<prometheus::Counter>* context_switches_family_;

    // 系统和进程运行时间
    prometheus::Family<prometheus::Counter>* uptime_family_;
};