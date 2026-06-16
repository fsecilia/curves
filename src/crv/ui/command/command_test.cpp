// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "command.hpp"
#include <crv/test/test.hpp>

namespace crv::command {
namespace {

//
// nonmergeable_command_t
//

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

//
// mergeable_command_t
//

struct command_mergeable_command_test_t : Test
{
    struct sut_t final : mergeable_command_t<sut_t>
    {
        bool will_merge = true;

        bool merged = false;
        bool dcast_succeeded = false;

        auto redo() -> void override {}
        auto undo() -> void override {}
        auto merge_timeout() const noexcept -> duration_t override { return duration_t{0}; }

    protected:
        using mergeable_command_t<sut_t>::try_merge;

        auto try_merge(sut_t&&) noexcept -> bool override
        {
            dcast_succeeded = true;
            if (!will_merge) return false;
            merged = true;
            return true;
        }
    };
    sut_t sut_impl;
    command_i& sut = sut_impl;
};

TEST_F(command_mergeable_command_test_t, mismatched_type_returns_false)
{
    struct other_command_t final : mergeable_command_t<other_command_t>
    {
        auto redo() -> void override {}
        auto undo() -> void override {}
        auto merge_timeout() const noexcept -> duration_t override { return duration_t::zero(); }

    protected:
        using mergeable_command_t<other_command_t>::try_merge;
        auto try_merge(other_command_t&&) noexcept -> bool override { return false; }
    };
    other_command_t other_typed_command_impl;
    command_i& other_typed_command = other_typed_command_impl;

    // dcast fails; strongly-typed overload is never reached
    EXPECT_FALSE(sut.try_merge(std::move(other_typed_command)));
    EXPECT_FALSE(sut_impl.dcast_succeeded);
    EXPECT_FALSE(sut_impl.merged);
}

TEST_F(command_mergeable_command_test_t, matched_type_fails_fold_returns_false)
{
    // reject the merge
    sut_impl.will_merge = false;

    auto other_sut = sut_t{};

    // dcast succeeds, overload is called, but rejects the merge
    EXPECT_FALSE(sut.try_merge(std::move(other_sut)));
    EXPECT_TRUE(sut_impl.dcast_succeeded);
    EXPECT_FALSE(sut_impl.merged);
}

TEST_F(command_mergeable_command_test_t, matched_type_succeeds_fold_returns_true)
{
    auto other_sut = sut_t{};

    // dcast succeeds, overload is called, accepts the merge
    EXPECT_TRUE(sut.try_merge(std::move(other_sut)));
    EXPECT_TRUE(sut_impl.dcast_succeeded);
    EXPECT_TRUE(sut_impl.merged);
}

} // namespace
} // namespace crv::command
