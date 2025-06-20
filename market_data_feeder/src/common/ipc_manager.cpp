#include "common/ipc_manager.h"
#include "common/logger.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <signal.h>
#include <sys/wait.h>

namespace market_feeder {

IPCManager& IPCManager::getInstance() {
    static IPCManager instance;
    return instance;
}

IPCManager::~IPCManager() {
    cleanup();
}

bool IPCManager::initialize(bool is_master) {
    is_master_ = is_master;
    
    if (is_master) {
        return initializeMaster();
    } else {
        return initializeWorker();
    }
}

bool IPCManager::initializeMaster() {
    LOG_INFO("Initializing IPC manager for master process");
    
    // 创建消息队列
    if (!createMessageQueue()) {
        LOG_ERROR("Failed to create message queue");
        return false;
    }
    
    // 创建共享内存
    if (!createSharedMemory()) {
        LOG_ERROR("Failed to create shared memory");
        return false;
    }
    
    // 创建信号量
    if (!createSemaphores()) {
        LOG_ERROR("Failed to create semaphores");
        return false;
    }
    
    // 初始化共享内存数据
    initializeSharedData();
    
    // 设置信号处理
    setupSignalHandlers();
    
    initialized_ = true;
    LOG_INFO("IPC manager initialized successfully for master process");
    
    return true;
}

bool IPCManager::initializeWorker() {
    LOG_INFO("Initializing IPC manager for worker process");
    
    // 连接到现有的消息队列
    if (!connectMessageQueue()) {
        LOG_ERROR("Failed to connect to message queue");
        return false;
    }
    
    // 连接到现有的共享内存
    if (!connectSharedMemory()) {
        LOG_ERROR("Failed to connect to shared memory");
        return false;
    }
    
    // 连接到现有的信号量
    if (!connectSemaphores()) {
        LOG_ERROR("Failed to connect to semaphores");
        return false;
    }
    
    // 设置信号处理
    setupSignalHandlers();
    
    initialized_ = true;
    LOG_INFO("IPC manager initialized successfully for worker process");
    
    return true;
}

bool IPCManager::createMessageQueue() {
    // 创建消息队列
    key_t key = ftok(constants::IPC_KEY_FILE.c_str(), constants::MSG_QUEUE_ID);
    if (key == -1) {
        LOG_ERROR("Failed to generate key for message queue: {}", strerror(errno));
        return false;
    }
    
    msg_queue_id_ = msgget(key, IPC_CREAT | IPC_EXCL | 0666);
    if (msg_queue_id_ == -1) {
        if (errno == EEXIST) {
            // 队列已存在，删除后重新创建
            msg_queue_id_ = msgget(key, 0666);
            if (msg_queue_id_ != -1) {
                msgctl(msg_queue_id_, IPC_RMID, nullptr);
            }
            msg_queue_id_ = msgget(key, IPC_CREAT | 0666);
        }
        
        if (msg_queue_id_ == -1) {
            LOG_ERROR("Failed to create message queue: {}", strerror(errno));
            return false;
        }
    }
    
    LOG_DEBUG("Message queue created with ID: {}", msg_queue_id_);
    return true;
}

bool IPCManager::connectMessageQueue() {
    key_t key = ftok(constants::IPC_KEY_FILE.c_str(), constants::MSG_QUEUE_ID);
    if (key == -1) {
        LOG_ERROR("Failed to generate key for message queue: {}", strerror(errno));
        return false;
    }
    
    msg_queue_id_ = msgget(key, 0666);
    if (msg_queue_id_ == -1) {
        LOG_ERROR("Failed to connect to message queue: {}", strerror(errno));
        return false;
    }
    
    LOG_DEBUG("Connected to message queue with ID: {}", msg_queue_id_);
    return true;
}

bool IPCManager::createSharedMemory() {
    // 创建共享内存
    key_t key = ftok(constants::IPC_KEY_FILE.c_str(), constants::SHM_ID);
    if (key == -1) {
        LOG_ERROR("Failed to generate key for shared memory: {}", strerror(errno));
        return false;
    }
    
    size_t shm_size = sizeof(SharedMemoryData);
    shm_id_ = shmget(key, shm_size, IPC_CREAT | IPC_EXCL | 0666);
    if (shm_id_ == -1) {
        if (errno == EEXIST) {
            // 共享内存已存在，删除后重新创建
            shm_id_ = shmget(key, shm_size, 0666);
            if (shm_id_ != -1) {
                shmctl(shm_id_, IPC_RMID, nullptr);
            }
            shm_id_ = shmget(key, shm_size, IPC_CREAT | 0666);
        }
        
        if (shm_id_ == -1) {
            LOG_ERROR("Failed to create shared memory: {}", strerror(errno));
            return false;
        }
    }
    
    // 映射共享内存
    shared_data_ = static_cast<SharedMemoryData*>(shmat(shm_id_, nullptr, 0));
    if (shared_data_ == reinterpret_cast<SharedMemoryData*>(-1)) {
        LOG_ERROR("Failed to attach shared memory: {}", strerror(errno));
        return false;
    }
    
    LOG_DEBUG("Shared memory created with ID: {}, size: {} bytes", shm_id_, shm_size);
    return true;
}

bool IPCManager::connectSharedMemory() {
    key_t key = ftok(constants::IPC_KEY_FILE.c_str(), constants::SHM_ID);
    if (key == -1) {
        LOG_ERROR("Failed to generate key for shared memory: {}", strerror(errno));
        return false;
    }
    
    shm_id_ = shmget(key, sizeof(SharedMemoryData), 0666);
    if (shm_id_ == -1) {
        LOG_ERROR("Failed to connect to shared memory: {}", strerror(errno));
        return false;
    }
    
    // 映射共享内存
    shared_data_ = static_cast<SharedMemoryData*>(shmat(shm_id_, nullptr, 0));
    if (shared_data_ == reinterpret_cast<SharedMemoryData*>(-1)) {
        LOG_ERROR("Failed to attach shared memory: {}", strerror(errno));
        return false;
    }
    
    LOG_DEBUG("Connected to shared memory with ID: {}", shm_id_);
    return true;
}

bool IPCManager::createSemaphores() {
    // 创建信号量集
    key_t key = ftok(constants::IPC_KEY_FILE.c_str(), constants::SEM_ID);
    if (key == -1) {
        LOG_ERROR("Failed to generate key for semaphores: {}", strerror(errno));
        return false;
    }
    
    sem_id_ = semget(key, constants::SEM_COUNT, IPC_CREAT | IPC_EXCL | 0666);
    if (sem_id_ == -1) {
        if (errno == EEXIST) {
            // 信号量已存在，删除后重新创建
            sem_id_ = semget(key, constants::SEM_COUNT, 0666);
            if (sem_id_ != -1) {
                semctl(sem_id_, 0, IPC_RMID);
            }
            sem_id_ = semget(key, constants::SEM_COUNT, IPC_CREAT | 0666);
        }
        
        if (sem_id_ == -1) {
            LOG_ERROR("Failed to create semaphores: {}", strerror(errno));
            return false;
        }
    }
    
    // 初始化信号量值
    union semun {
        int val;
        struct semid_ds *buf;
        unsigned short *array;
    } sem_union;
    
    // 初始化互斥信号量为1
    sem_union.val = 1;
    for (int i = 0; i < constants::SEM_COUNT; ++i) {
        if (semctl(sem_id_, i, SETVAL, sem_union) == -1) {
            LOG_ERROR("Failed to initialize semaphore {}: {}", i, strerror(errno));
            return false;
        }
    }
    
    LOG_DEBUG("Semaphores created with ID: {}, count: {}", sem_id_, constants::SEM_COUNT);
    return true;
}

bool IPCManager::connectSemaphores() {
    key_t key = ftok(constants::IPC_KEY_FILE.c_str(), constants::SEM_ID);
    if (key == -1) {
        LOG_ERROR("Failed to generate key for semaphores: {}", strerror(errno));
        return false;
    }
    
    sem_id_ = semget(key, constants::SEM_COUNT, 0666);
    if (sem_id_ == -1) {
        LOG_ERROR("Failed to connect to semaphores: {}", strerror(errno));
        return false;
    }
    
    LOG_DEBUG("Connected to semaphores with ID: {}", sem_id_);
    return true;
}

void IPCManager::initializeSharedData() {
    if (!shared_data_) {
        return;
    }
    
    // 初始化共享内存数据
    memset(shared_data_, 0, sizeof(SharedMemoryData));
    shared_data_->master_pid = getpid();
    shared_data_->start_time = time(nullptr);
    shared_data_->shutdown_flag = false;
    shared_data_->reload_flag = false;
    shared_data_->worker_count = 0;
    
    LOG_DEBUG("Shared memory data initialized");
}

void IPCManager::setupSignalHandlers() {
    // 忽略SIGPIPE信号
    signal(SIGPIPE, SIG_IGN);
    
    // 设置其他信号处理器（如果需要）
    // 注意：在实际应用中，应该使用signalfd或其他现代信号处理方式
}

bool IPCManager::sendMessage(const IPCMessage& message) {
    if (!initialized_ || msg_queue_id_ == -1) {
        LOG_ERROR("IPC manager not initialized or message queue not available");
        return false;
    }
    
    struct {
        long mtype;
        IPCMessage msg;
    } msg_buf;
    
    msg_buf.mtype = static_cast<long>(message.type);
    msg_buf.msg = message;
    
    if (msgsnd(msg_queue_id_, &msg_buf, sizeof(IPCMessage), IPC_NOWAIT) == -1) {
        if (errno != EAGAIN) {
            LOG_ERROR("Failed to send message: {}", strerror(errno));
        }
        return false;
    }
    
    LOG_TRACE("Message sent: type={}, from={}, to={}", 
              static_cast<int>(message.type), message.sender_pid, message.receiver_pid);
    return true;
}

bool IPCManager::receiveMessage(IPCMessage& message, MessageType type, bool blocking) {
    if (!initialized_ || msg_queue_id_ == -1) {
        LOG_ERROR("IPC manager not initialized or message queue not available");
        return false;
    }
    
    struct {
        long mtype;
        IPCMessage msg;
    } msg_buf;
    
    int flags = blocking ? 0 : IPC_NOWAIT;
    long msg_type = (type == MessageType::ANY) ? 0 : static_cast<long>(type);
    
    ssize_t result = msgrcv(msg_queue_id_, &msg_buf, sizeof(IPCMessage), msg_type, flags);
    if (result == -1) {
        if (errno != ENOMSG && errno != EAGAIN) {
            LOG_ERROR("Failed to receive message: {}", strerror(errno));
        }
        return false;
    }
    
    message = msg_buf.msg;
    LOG_TRACE("Message received: type={}, from={}, to={}", 
              static_cast<int>(message.type), message.sender_pid, message.receiver_pid);
    return true;
}

bool IPCManager::lockSharedMemory(int sem_index) {
    if (!initialized_ || sem_id_ == -1) {
        return false;
    }
    
    struct sembuf sem_op;
    sem_op.sem_num = sem_index;
    sem_op.sem_op = -1;  // P操作（减1）
    sem_op.sem_flg = SEM_UNDO;
    
    if (semop(sem_id_, &sem_op, 1) == -1) {
        LOG_ERROR("Failed to lock semaphore {}: {}", sem_index, strerror(errno));
        return false;
    }
    
    return true;
}

bool IPCManager::unlockSharedMemory(int sem_index) {
    if (!initialized_ || sem_id_ == -1) {
        return false;
    }
    
    struct sembuf sem_op;
    sem_op.sem_num = sem_index;
    sem_op.sem_op = 1;   // V操作（加1）
    sem_op.sem_flg = SEM_UNDO;
    
    if (semop(sem_id_, &sem_op, 1) == -1) {
        LOG_ERROR("Failed to unlock semaphore {}: {}", sem_index, strerror(errno));
        return false;
    }
    
    return true;
}

bool IPCManager::addWorkerProcess(pid_t pid, int worker_id) {
    if (!lockSharedMemory(constants::SEM_WORKER_LIST)) {
        return false;
    }
    
    bool success = false;
    if (shared_data_ && shared_data_->worker_count < constants::MAX_WORKER_PROCESSES) {
        ProcessInfo& info = shared_data_->workers[shared_data_->worker_count];
        info.pid = pid;
        info.worker_id = worker_id;
        info.status = ProcessStatus::STARTING;
        info.start_time = time(nullptr);
        info.last_heartbeat = time(nullptr);
        
        shared_data_->worker_count++;
        success = true;
        
        LOG_INFO("Worker process added: pid={}, worker_id={}", pid, worker_id);
    }
    
    unlockSharedMemory(constants::SEM_WORKER_LIST);
    return success;
}

bool IPCManager::removeWorkerProcess(pid_t pid) {
    if (!lockSharedMemory(constants::SEM_WORKER_LIST)) {
        return false;
    }
    
    bool success = false;
    if (shared_data_) {
        for (int i = 0; i < shared_data_->worker_count; ++i) {
            if (shared_data_->workers[i].pid == pid) {
                // 移动后面的元素
                for (int j = i; j < shared_data_->worker_count - 1; ++j) {
                    shared_data_->workers[j] = shared_data_->workers[j + 1];
                }
                shared_data_->worker_count--;
                success = true;
                
                LOG_INFO("Worker process removed: pid={}", pid);
                break;
            }
        }
    }
    
    unlockSharedMemory(constants::SEM_WORKER_LIST);
    return success;
}

bool IPCManager::updateWorkerStatus(pid_t pid, ProcessStatus status) {
    if (!lockSharedMemory(constants::SEM_WORKER_LIST)) {
        return false;
    }
    
    bool success = false;
    if (shared_data_) {
        for (int i = 0; i < shared_data_->worker_count; ++i) {
            if (shared_data_->workers[i].pid == pid) {
                shared_data_->workers[i].status = status;
                shared_data_->workers[i].last_heartbeat = time(nullptr);
                success = true;
                break;
            }
        }
    }
    
    unlockSharedMemory(constants::SEM_WORKER_LIST);
    return success;
}

bool IPCManager::updateStatistics(const ProcessStatistics& stats) {
    if (!lockSharedMemory(constants::SEM_STATISTICS)) {
        return false;
    }
    
    if (shared_data_) {
        shared_data_->stats = stats;
        shared_data_->stats.last_update = time(nullptr);
    }
    
    unlockSharedMemory(constants::SEM_STATISTICS);
    return true;
}

ProcessStatistics IPCManager::getStatistics() {
    ProcessStatistics stats = {};
    
    if (lockSharedMemory(constants::SEM_STATISTICS)) {
        if (shared_data_) {
            stats = shared_data_->stats;
        }
        unlockSharedMemory(constants::SEM_STATISTICS);
    }
    
    return stats;
}

std::vector<ProcessInfo> IPCManager::getWorkerProcesses() {
    std::vector<ProcessInfo> workers;
    
    if (lockSharedMemory(constants::SEM_WORKER_LIST)) {
        if (shared_data_) {
            for (int i = 0; i < shared_data_->worker_count; ++i) {
                workers.push_back(shared_data_->workers[i]);
            }
        }
        unlockSharedMemory(constants::SEM_WORKER_LIST);
    }
    
    return workers;
}

void IPCManager::setShutdownFlag(bool flag) {
    if (shared_data_) {
        shared_data_->shutdown_flag = flag;
    }
}

bool IPCManager::getShutdownFlag() {
    return shared_data_ ? shared_data_->shutdown_flag : false;
}

void IPCManager::setReloadFlag(bool flag) {
    if (shared_data_) {
        shared_data_->reload_flag = flag;
    }
}

bool IPCManager::getReloadFlag() {
    return shared_data_ ? shared_data_->reload_flag : false;
}

void IPCManager::cleanup() {
    if (!initialized_) {
        return;
    }
    
    LOG_INFO("Cleaning up IPC resources...");
    
    // 分离共享内存
    if (shared_data_ && shared_data_ != reinterpret_cast<SharedMemoryData*>(-1)) {
        shmdt(shared_data_);
        shared_data_ = nullptr;
    }
    
    // 如果是主进程，删除IPC资源
    if (is_master_) {
        if (msg_queue_id_ != -1) {
            msgctl(msg_queue_id_, IPC_RMID, nullptr);
            LOG_DEBUG("Message queue removed");
        }
        
        if (shm_id_ != -1) {
            shmctl(shm_id_, IPC_RMID, nullptr);
            LOG_DEBUG("Shared memory removed");
        }
        
        if (sem_id_ != -1) {
            semctl(sem_id_, 0, IPC_RMID);
            LOG_DEBUG("Semaphores removed");
        }
    }
    
    msg_queue_id_ = -1;
    shm_id_ = -1;
    sem_id_ = -1;
    initialized_ = false;
    
    LOG_INFO("IPC cleanup completed");
}

} // namespace market_feeder