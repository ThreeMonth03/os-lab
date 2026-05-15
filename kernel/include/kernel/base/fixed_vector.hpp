#pragma once

#include <stddef.h>

#include "kernel/base/placement_new.hpp"

namespace kernel
{

template <typename T, size_t Capacity>
class FixedVector
{
public:
    FixedVector() = default;

    FixedVector(const FixedVector & other) { copy_from(other); }

    FixedVector(FixedVector && other) { move_from(other); }

    FixedVector & operator=(const FixedVector & other)
    {
        if (this != &other)
        {
            clear();
            copy_from(other);
        }

        return *this;
    }

    FixedVector & operator=(FixedVector && other)
    {
        if (this != &other)
        {
            clear();
            move_from(other);
        }

        return *this;
    }

    ~FixedVector() { clear(); }

    size_t size() const { return size_; }
    constexpr size_t capacity() const { return Capacity; }
    bool empty() const { return size_ == 0; }
    bool full() const { return size_ == Capacity; }

    T * data() { return slot(0); }
    const T * data() const { return slot(0); }

    T * begin() { return data(); }
    T * end() { return data() + size_; }
    const T * begin() const { return data(); }
    const T * end() const { return data() + size_; }

    T & operator[](size_t index) { return *slot(index); }
    const T & operator[](size_t index) const { return *slot(index); }
    T & front() { return (*this)[0]; }
    const T & front() const { return (*this)[0]; }
    T & back() { return (*this)[size_ - 1]; }
    const T & back() const { return (*this)[size_ - 1]; }

    [[nodiscard]] bool push_back(const T & value) { return emplace_back(value) != nullptr; }
    [[nodiscard]] bool push_back(T && value) { return emplace_back(static_cast<T &&>(value)) != nullptr; }

    template <typename... Args>
    [[nodiscard]] T * emplace_back(Args &&... args)
    {
        if (full())
        {
            return nullptr;
        }

        T * value = slot(size_);
        new (value) T(static_cast<Args &&>(args)...);
        ++size_;
        return value;
    }

    bool pop_back()
    {
        if (empty())
        {
            return false;
        }

        --size_;
        slot(size_)->~T();
        return true;
    }

    void clear()
    {
        while (!empty())
        {
            pop_back();
        }
    }

private:
    void copy_from(const FixedVector & other)
    {
        for (const T & value : other)
        {
            if (!push_back(value))
            {
                return;
            }
        }
    }

    void move_from(FixedVector & other)
    {
        for (T & value : other)
        {
            if (emplace_back(static_cast<T &&>(value)) == nullptr)
            {
                return;
            }
        }
        other.clear();
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
    size_t size_ = 0;
};

} // namespace kernel
