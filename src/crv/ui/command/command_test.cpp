// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "command.hpp"
#include <crv/test/test.hpp>

namespace crv::command {
namespace {

struct command_nonmergeable_command_test_t : Test
{
    struct sut_t : nonmergeable_command_t
    {
        auto redo() -> void override {}
        auto undo() -> void override {}
    };

    sut_t sut;
};

TEST_F(command_nonmergeable_command_test_t, timeout)
{
    EXPECT_EQ(sut_t::duration_t::zero(), sut.merge_timeout());
}

TEST_F(command_nonmergeable_command_test_t, try_merge)
{
    auto copy = sut;
    EXPECT_FALSE(sut.try_merge(std::move(copy)));
}

} // namespace
} // namespace crv::command
