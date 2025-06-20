#pragma once

#include "types.h"
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <signal.h>
#include <memory>
#include <functional>
#include <map>
#include <mutex>

namespace market_feeder {

// IPC消息类型
enum class IPCMessageType {
    HEARTBEAT = 1,      // 心跳消息
    SHUTDOWN = 2,       // 关闭消息
    RELOAD_CONFIG = 3,  // 重载配置
    STATISTICS = 4,     // 统计信息
    MARKET_DATA = 5,    // 市场数据
    ERROR_REPORT = 6    // 错误报告
};

// IPC消息结构
struct IPCMessage {
    long msg_type;              // 消息类型
    IPCMessageType ipc_type;    // IPC消息类型
    pid_t sender_pid;           // 发送者PID
    size_t data_size;           // 数据大小
    char data[4096];            // 消息数据
    
    IPCMessage() : msg_type(1), ipc_type(IPCMessageType::HEARTBEAT),
                  sender_pid(0), data_size(0) {
        memset(data, 0, sizeof(data));
    }
};

// 共享内存数据结构
struct SharedMemoryData {
    // 进程信息数组
    ProcessInfo processes[constants::MAX_WORKER_PROCESSES];
    int process_count;
    
    // 全局统计信息
    Statistics global_stats;
    
    // 配置版本号
    uint64_t config_version;
    
    // 控制标志
    volatile bool shutdown_flag;
    volatile bool reload_config_flag;
    
    // 互斥锁和条件变量（使用进程间同步）
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    
    SharedMemoryData() : process_count(0), config_version(0),
                        shutdown_flag(false), reload_config_flag(false) {
        memset(processes, 0, sizeof(processes));
        
        // 初始化进程间互斥锁
        pthread_mutexattr_t mutex_attr;
        pthread_mutexattr_init(&mutex_attr);
        pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(&mutex, &mutex_attr);
        pthread_mutexattr_destroy(&mutex_attr);
        
        // 初始化进程间条件变量
        pthread_condattr_t cond_attr;
        pthread_condattr_init(&cond_attr);
        pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);
        pthread_cond_init(&condition, &cond_attr);
        pthread_condattr_destroy(&cond_attr);
    }
    
    ~SharedMemoryData() {
        pthread_mutex_destroy(&mutex);
        pthread_cond_destroy(&condition);
    }
};

class IPCManager {
public:
    static IPCManager& getInstance();
    
    // 初始化IPC系统
    bool initialize(ProcessType process_type);
    
    // 清理IPC资源
    void cleanup();
    
    // 消息队列操作
    bool sendMessage(const IPCMessage& message, pid_t target_pid = 0);
    bool receiveMessage(IPCMessage& message, IPCMessageType type = IPCMessageType::HEARTBEAT, bool blocking = true);
    
    // 共享内存操作
    SharedMemoryData* getSharedMemory();
    bool lockSharedMemory();
    bool unlockSharedMemory();
    
    // 信号处理
    void setupSignalHandlers();
    void registerSignalHandler(int signal, std::function<void(int)> handler);
    
    // 进程管理
    bool registerProcess(const ProcessInfo& process_info);
    bool unregisterProcess(pid_t pid);
    bool updateProcessStatus(pid_t pid, ProcessStatus status);
    bool updateProcessHeartbeat(pid_t pid);
    std::vector<ProcessInfo> getAllProcesses();
    
    // 统计信息
    bool updateStatistics(const Statistics& stats);
    Statistics getGlobalStatistics();
    
    // 控制操作
    void setShutdownFlag(bool flag);
    bool getShutdownFlag();
    void setReloadConfigFlag(bool flag);
    bool getReloadConfigFlag();
    
    // 广播消息给所有工作进程
    bool broadcastMessage(const IPCMessage& message);
    
private:
    IPCManager() = default;
    ~IPCManager();
    IPCManager(const IPCManager&) = delete;
    IPCManager& operator=(const IPCManager&) = delete;
    
    // 创建IPC资源
    bool createMessageQueue();
    bool createSharedMemory();
    bool createSemaphore();
    
    // 销毁IPC资源
    void destroyMessageQueue();
    void destroySharedMemory();
    void destroySemaphore();
    
    // 信号处理函数
    static void signalHandler(int signal);
    
private:
    ProcessType process_type_ = ProcessType::MASTER;
    
    // 消息队列
    int msg_queue_id_ = -1;
    key_t msg_queue_key_;
    
    // 共享内存
    int shm_id_ = -1;
    key_t shm_key_;
    SharedMemoryData* shared_memory_ = nullptr;
    
    // 信号量
    int sem_id_ = -1;
    key_t sem_key_;
    
    // 信号处理器映射
    static std::map<int, std::function<void(int)>> signal_handlers_;
    static std::mutex signal_mutex_;
    
    bool initialized_ = false;
    mutable std::mutex ipc_mutex_;
};

} // namespace market_feeder