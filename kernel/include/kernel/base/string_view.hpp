#pragma once

#include <stddef.h>

namespace kernel {

class StringView {
  public:
    static constexpr size_t npos = static_cast<size_t>(-1);

    constexpr StringView() = default;

    constexpr StringView(const char* value)
        : data_(value == nullptr ? "" : value), size_(length(value)) {}

    constexpr StringView(const char* data, size_t size)
        : data_(data == nullptr ? "" : data), size_(data == nullptr ? 0 : size) {}

    [[nodiscard]] constexpr const char* data() const { return data_; }
    [[nodiscard]] constexpr size_t size() const { return size_; }
    [[nodiscard]] constexpr bool empty() const { return size_ == 0; }

    [[nodiscard]] constexpr const char* begin() const { return data_; }
    [[nodiscard]] constexpr const char* end() const { return data_ + size_; }

    [[nodiscard]] constexpr char operator[](size_t index) const { return data_[index]; }
    [[nodiscard]] constexpr char front() const { return data_[0]; }
    [[nodiscard]] constexpr char back() const { return data_[size_ - 1]; }

    constexpr void remove_prefix(size_t count) {
        if (count > size_) {
            count = size_;
        }

        data_ += count;
        size_ -= count;
    }

    constexpr void remove_suffix(size_t count) {
        if (count > size_) {
            count = size_;
        }

        size_ -= count;
    }

    [[nodiscard]] constexpr StringView substr(size_t offset, size_t count = npos) const {
        if (offset > size_) {
            offset = size_;
        }

        const size_t remaining = size_ - offset;
        if (count > remaining) {
            count = remaining;
        }

        return {data_ + offset, count};
    }

    [[nodiscard]] constexpr bool starts_with(StringView prefix) const {
        if (prefix.size_ > size_) {
            return false;
        }

        for (size_t index = 0; index < prefix.size_; ++index) {
            if (data_[index] != prefix.data_[index]) {
                return false;
            }
        }

        return true;
    }

  private:
    [[nodiscard]] static constexpr size_t length(const char* value) {
        if (value == nullptr) {
            return 0;
        }

        size_t size = 0;
        while (value[size] != '\0') {
            ++size;
        }
        return size;
    }

    const char* data_ = "";
    size_t size_ = 0;
};

[[nodiscard]] constexpr bool operator==(StringView left, StringView right) {
    if (left.size() != right.size()) {
        return false;
    }

    for (size_t index = 0; index < left.size(); ++index) {
        if (left[index] != right[index]) {
            return false;
        }
    }

    return true;
}

[[nodiscard]] constexpr bool operator!=(StringView left, StringView right) {
    return !(left == right);
}

} // namespace kernel
