#pragma once

#include <stddef.h>

namespace kernel
{

template <typename T>
class Span
{
public:
    constexpr Span() = default;

    constexpr Span(T * data, size_t size)
        : data_(data)
        , size_(data == nullptr ? 0 : size)
    {
    }

    template <size_t Count>
    constexpr Span(T (&values)[Count])
        : data_(values)
        , size_(Count)
    {
    }

    constexpr T * data() const { return data_; }
    constexpr size_t size() const { return size_; }
    constexpr size_t size_bytes() const { return size_ * sizeof(T); }
    constexpr bool empty() const { return size_ == 0; }

    constexpr T * begin() const { return data_; }
    constexpr T * end() const { return data_ + size_; }

    constexpr T & operator[](size_t index) const { return data_[index]; }
    constexpr T & front() const { return data_[0]; }
    constexpr T & back() const { return data_[size_ - 1]; }

    [[nodiscard]] constexpr Span first(size_t count) const
    {
        if (count > size_)
        {
            count = size_;
        }

        return {data_, count};
    }

    [[nodiscard]] constexpr Span last(size_t count) const
    {
        if (count > size_)
        {
            count = size_;
        }

        return {data_ + (size_ - count), count};
    }

    [[nodiscard]] constexpr Span subspan(size_t offset,
                                         size_t count = static_cast<size_t>(-1)) const
    {
        if (offset > size_)
        {
            offset = size_;
        }

        const size_t remaining = size_ - offset;
        if (count > remaining)
        {
            count = remaining;
        }

        return {data_ + offset, count};
    }

private:
    T * data_ = nullptr;
    size_t size_ = 0;
};

} // namespace kernel
