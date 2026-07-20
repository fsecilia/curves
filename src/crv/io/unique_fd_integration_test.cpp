// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "unique_fd.hpp"
#include <crv/test/test.hpp>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

namespace crv {
namespace {

struct unique_fd_integration_test_t : Test
{
    using sut_t = unique_fd_t;

    [[nodiscard]] static auto open_fd() noexcept -> int { return ::open("/dev/null", O_RDONLY | O_CLOEXEC); }

    [[nodiscard]] static auto is_open(int fd) noexcept -> bool
    {
        errno = 0;
        auto const result = fcntl(fd, F_GETFD);

        // failure other than EBADF does not prove that the descriptor has been closed
        return result != -1 || errno != EBADF;
    }

    int fd;
    auto SetUp() -> void override
    {
        fd = open_fd();
        ASSERT_GE(fd, 0);
        ASSERT_TRUE(is_open(fd));
    }
    auto TearDown() -> void override
    {
        EXPECT_FALSE(is_open(fd));
        if (is_open(fd)) static_cast<void>(close(fd));
    }
};

static_assert(std::is_default_constructible_v<unique_fd_integration_test_t::sut_t>);
static_assert(!std::is_copy_constructible_v<unique_fd_integration_test_t::sut_t>);
static_assert(!std::is_copy_assignable_v<unique_fd_integration_test_t::sut_t>);
static_assert(std::is_nothrow_move_constructible_v<unique_fd_integration_test_t::sut_t>);
static_assert(std::is_nothrow_move_assignable_v<unique_fd_integration_test_t::sut_t>);
static_assert(std::is_nothrow_destructible_v<unique_fd_integration_test_t::sut_t>);

TEST_F(unique_fd_integration_test_t, destructor_closes_fd)
{
    static_cast<void>(sut_t{fd});
}

TEST_F(unique_fd_integration_test_t, move_construction_transfers_close_ownership)
{
    auto src = sut_t{fd};

    static_cast<void>(sut_t{std::move(src)});
}

TEST_F(unique_fd_integration_test_t, move_assign_into_armed_closes_src_fd)
{
    auto src = sut_t{fd};
    auto dst = sut_t{};

    auto& actual = (dst = std::move(src));

    EXPECT_EQ(&actual, &dst);
    EXPECT_TRUE(is_open(fd));
}

TEST_F(unique_fd_integration_test_t, move_assignment_from_disarmed_closes_destination_fd)
{
    auto src = sut_t{};
    auto dst = sut_t{fd};

    auto& actual = (dst = std::move(src));

    EXPECT_EQ(&actual, &dst);
}

TEST_F(unique_fd_integration_test_t, move_assignment_replaces_owned_fd)
{
    auto const src_fd = open_fd();
    ASSERT_GE(src_fd, 0);
    EXPECT_TRUE(is_open(src_fd));

    {
        auto src = sut_t{src_fd};
        auto dst = sut_t{fd};

        dst = std::move(src);

        EXPECT_FALSE(is_open(fd));
        EXPECT_TRUE(is_open(src_fd));
        EXPECT_FALSE(src);
        EXPECT_EQ(src_fd, dst.get());
    }

    EXPECT_FALSE(is_open(src_fd));
}

TEST_F(unique_fd_integration_test_t, reset_without_fd_closes_owned_fd)
{
    auto sut = sut_t{fd};

    sut.reset();
}

TEST_F(unique_fd_integration_test_t, file_is_closed_after_reset_disarmed)
{
    auto sut = sut_t{};

    sut.reset(fd);

    EXPECT_TRUE(is_open(fd));
}

TEST_F(unique_fd_integration_test_t, release_returns_fd_without_closing_it)
{
    EXPECT_EQ(fd, sut_t{fd}.release());

    EXPECT_TRUE(is_open(fd));
    close(fd);
}

TEST_F(unique_fd_integration_test_t, reset_replaces_owned_fd)
{
    auto const new_fd = open_fd();
    ASSERT_GE(new_fd, 0);
    EXPECT_TRUE(is_open(new_fd));

    {
        auto sut = sut_t{fd};

        sut.reset(new_fd);

        EXPECT_FALSE(is_open(fd));
        EXPECT_TRUE(is_open(new_fd));
    }

    EXPECT_FALSE(is_open(new_fd));
}

} // namespace
} // namespace crv
