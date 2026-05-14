#pragma once

#include <gtest/gtest.h>

#include "kernel/base/string_view.hpp"

namespace os_lab::test
{

inline void expect_text(kernel::StringView actual, kernel::StringView expected)
{
    EXPECT_EQ(actual.size(), expected.size());
    EXPECT_TRUE(actual == expected);
}

} // namespace os_lab::test
