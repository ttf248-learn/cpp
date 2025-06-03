#pragma once

#include <map>
#include <mutex>
#include <string>

template<typename Key, typename Value>
class ThreadSafeMap
{
public:
    // 默认构造函数
    ThreadSafeMap() = default;

    // 拷贝构造函数
    ThreadSafeMap(const ThreadSafeMap &other)
    {
        std::lock_guard<std::mutex> lock(other.mtx_);
        map_ = other.map_;
    }

    // 移动构造函数
    ThreadSafeMap(ThreadSafeMap &&other) noexcept
    {
        std::lock_guard<std::mutex> lock(other.mtx_);
        map_ = std::move(other.map_);
    }

    // 插入或更新键值对
    void insert(const Key &key, const Value &value)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        map_[key] = value;
    }

    // 获取值
    bool get(const Key &key, Value &value) const
    {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = map_.find(key);
        if (it != map_.end())
        {
            value = it->second;
            return true;
        }
        return false;
    }

    // 删除键值对
    void erase(const Key &key)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        map_.erase(key);
    }

    // 检查键是否存在
    bool contains(const Key &key) const
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return map_.find(key) != map_.end();
    }

    // 操作符号[]，返回实际存储对象的引用
    Value &operator[](const Key &key)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return map_[key];
    }

    // emplace接口
    template<typename... Args>
    std::pair<typename std::map<Key, Value>::iterator, bool> emplace(Args &&...args)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return map_.emplace(std::forward<Args>(args)...);
    }

    // 清空接口
    void clear()
    {
        std::lock_guard<std::mutex> lock(mtx_);
        map_.clear();
    }

    // 获取大小
    size_t size() const
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return map_.size();
    }

    // 获取底层map的副本
    std::map<Key, Value> get_map_copy() const
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return map_;
    }

private:
    mutable std::mutex mtx_;
    std::map<Key, Value> map_;
};
