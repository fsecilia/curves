// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "mesher_workspace.hpp"
#include <crv/test/test.hpp>

namespace crv::generic {
namespace {

struct mesher_workspace_test_t : Test
{
    struct container_t
    {
        bool clear_called = false;
        bool reserve_called = false;

        std::size_t reserved_capacity = 0;

        constexpr auto clear() noexcept -> void { clear_called = true; }

        constexpr auto reserve(std::size_t capacity) -> void
        {
            reserve_called = true;
            reserved_capacity = capacity;
        }
    };

    using sut_t = mesher_workspace_t<container_t, container_t>;
};

TEST_F(mesher_workspace_test_t, clear_forwards_to_both_members)
{
    auto sut = sut_t{};

    sut.clear();

    EXPECT_TRUE(sut.result.clear_called);
    EXPECT_TRUE(sut.queue.clear_called);
}

TEST_F(mesher_workspace_test_t, reserve_forwards_to_both_members)
{
    auto sut = sut_t{};

    sut.reserve(256);

    EXPECT_TRUE(sut.result.reserve_called);
    EXPECT_EQ(sut.result.reserved_capacity, 256);

    EXPECT_TRUE(sut.queue.reserve_called);
    EXPECT_EQ(sut.queue.reserved_capacity, 256);
}

} // namespace
} // namespace crv::generic
