#pragma once

#include <stddef.h>

#include "kernel/placement_new.hpp"

namespace kernel {

template <typename T, size_t Capacity> class FixedVector {
  public:
    FixedVector() = default;

    FixedVector(const FixedVector& other) { copy_from(other); }

    FixedVector& operator=(const FixedVector& other) {
        if (this != &other) {
            clear();
            copy_from(other);
        }

        return *this;
    }

    ~FixedVector() { clear(); }

    [[nodiscard]] size_t size() const { return size_; }
    [[nodiscard]] constexpr size_t capacity() const { return Capacity; }
    [[nodiscard]] bool empty() const { return size_ == 0; }
    [[nodiscard]] bool full() const { return size_ == Capacity; }

    [[nodiscard]] T* data() { return slot(0); }
    [[nodiscard]] const T* data() const { return slot(0); }

    [[nodiscard]] T* begin() { return data(); }
    [[nodiscard]] T* end() { return data() + size_; }
    [[nodiscard]] const T* begin() const { return data(); }
    [[nodiscard]] const T* end() const { return data() + size_; }

    [[nodiscard]] T& operator[](size_t index) { return *slot(index); }
    [[nodiscard]] const T& operator[](size_t index) const { return *slot(index); }
    [[nodiscard]] T& front() { return (*this)[0]; }
    [[nodiscard]] const T& front() const { return (*this)[0]; }
    [[nodiscard]] T& back() { return (*this)[size_ - 1]; }
    [[nodiscard]] const T& back() const { return (*this)[size_ - 1]; }

    [[nodiscard]] bool push_back(const T& value) { return emplace_back(value) != nullptr; }

    template <typename... Args> [[nodiscard]] T* emplace_back(Args&&... args) {
        if (full()) {
            return nullptr;
        }

        T* value = slot(size_);
        new (value) T(static_cast<Args&&>(args)...);
        ++size_;
        return value;
    }

    bool pop_back() {
        if (empty()) {
            return false;
        }

        --size_;
        slot(size_)->~T();
        return true;
    }

    void clear() {
        while (!empty()) {
            pop_back();
        }
    }

  private:
    void copy_from(const FixedVector& other) {
        for (const T& value : other) {
            (void)push_back(value);
        }
    }

    [[nodiscard]] T* slot(size_t index) {
        return reinterpret_cast<T*>(storage_ + (sizeof(T) * index));
    }

    [[nodiscard]] const T* slot(size_t index) const {
        return reinterpret_cast<const T*>(storage_ + (sizeof(T) * index));
    }

    static constexpr size_t kStorageCapacity = Capacity == 0 ? 1 : Capacity;

    alignas(T) unsigned char storage_[sizeof(T) * kStorageCapacity] = {};
    size_t size_ = 0;
};

} // namespace kernel
