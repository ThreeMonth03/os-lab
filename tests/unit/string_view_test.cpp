#include <gtest/gtest.h>
#include "kernel/base/string_view.hpp"
#include "test_helpers.hpp"

namespace
{

using os_lab::test::expect_text;

TEST(StringViewTest, SupportsBasicViewOperations)
{
    const kernel::StringView text = "kernel";

    EXPECT_EQ(text.size(), 6u);
    EXPECT_FALSE(text.empty());
    EXPECT_TRUE(text.starts_with("ker"));
    expect_text(text.substr(1, 3), "ern");
    expect_text(text.substr(3), "nel");
}

} // namespace
