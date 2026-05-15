#pragma once

#include <stddef.h>

#include "kernel/base/placement_new.hpp"

namespace kernel
{

template <typename T, size_t Capacity>
class FixedQueue
{
public:
    FixedQueue() = default;

    FixedQueue(const FixedQueue &) = delete;
    FixedQueue & operator=(const FixedQueue &) = delete;
    FixedQueue(FixedQueue &&) = delete;
    FixedQueue & operator=(FixedQueue &&) = delete;

    ~FixedQueue() { clear(); }

    size_t size() const { return size_; }
    constexpr size_t capacity() const { return Capacity; }
    size_t available() const { return Capacity - size_; }
    bool empty() const { return size_ == 0; }
    bool full() const { return size_ == Capacity; }

    [[nodiscard]] bool push(const T & value)
    {
        if (full())
        {
            return false;
        }

        new (slot(tail_)) T(value);
        tail_ = advance(tail_);
        ++size_;
        return true;
    }

    [[nodiscard]] bool pop(T & value)
    {
        if (empty())
        {
            return false;
        }

        T * current = slot(head_);
        value = *current;
        current->~T();
        head_ = advance(head_);
        --size_;
        return true;
    }

    void clear()
    {
        while (!empty())
        {
            slot(head_)->~T();
            head_ = advance(head_);
            --size_;
        }
        tail_ = head_;
    }

private:
    size_t advance(size_t index) const
    {
        ++index;
        return index == Capacity ? 0 : index;
    }

    T * slot(size_t index)
    {
        return reinterpret_cast<T *>(storage_ + (sizeof(T) * index));
    }

    const T * slot(size_t index) const
    {
        return reinterpret_cast<const T *>(storage_ + (sizeof(T) * index));
    }

    static constexpr size_t kStorageCapacity = Capacity == 0 ? 1 : Capacity;

    alignas(T) unsigned char storage_[sizeof(T) * kStorageCapacity] = {};
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t size_ = 0;
};

} // namespace kernel
