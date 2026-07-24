// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "unique_fd.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv {
namespace {

//
// common fixture
//

struct unique_fd_test_t : Test
{
    struct mock_closer_t
    {
        virtual ~mock_closer_t() = default;
        MOCK_METHOD(void, call, (int), (const, noexcept));
    };
    StrictMock<mock_closer_t> mock_closer;
    StrictMock<mock_closer_t> new_mock_closer;

    struct closer_t
    {
        mock_closer_t* mock = nullptr;
        auto operator()(int fd) const noexcept -> void { mock->call(fd); }
    };

    using sut_t = generic::unique_fd_t<closer_t>;

    auto expect_close(int fd, mock_closer_t& mock) const noexcept -> void { EXPECT_CALL(mock, call(fd)); }
    auto expect_close(int fd) noexcept -> void { expect_close(fd, mock_closer); }
};

static_assert(std::is_default_constructible_v<unique_fd_test_t::closer_t>);
static_assert(noexcept(unique_fd_test_t::closer_t{}));
static_assert(std::copyable<unique_fd_test_t::closer_t>);
static_assert(std::is_nothrow_invocable_r_v<void, unique_fd_test_t::closer_t const&, int>);

static_assert(std::is_default_constructible_v<unique_fd_test_t::sut_t>);
static_assert(!std::is_copy_constructible_v<unique_fd_test_t::sut_t>);
static_assert(!std::is_copy_assignable_v<unique_fd_test_t::sut_t>);
static_assert(std::is_nothrow_move_constructible_v<unique_fd_test_t::sut_t>);
static_assert(std::is_nothrow_move_assignable_v<unique_fd_test_t::sut_t>);
static_assert(std::is_nothrow_destructible_v<unique_fd_test_t::sut_t>);

TEST_F(unique_fd_test_t, construction_preserves_negative_fd_as_disarmed)
{
    auto const fd = int{-2};

    auto sut = sut_t{fd};

    EXPECT_FALSE(sut);
    EXPECT_EQ(fd, sut.get());
}

TEST_F(unique_fd_test_t, move_assignment_transfers_closer_from_disarmed_source)
{
    auto const old_fd = int{17};
    auto const adopted_fd = int{23};

    auto src = sut_t{sut_t::disarmed_fd, closer_t{&new_mock_closer}};
    auto dst = sut_t{old_fd, closer_t{&mock_closer}};

    expect_close(old_fd);

    dst = std::move(src);
    dst.reset(adopted_fd);

    EXPECT_EQ(adopted_fd, dst.get());

    expect_close(adopted_fd, new_mock_closer);
}

TEST_F(unique_fd_test_t, release_returns_negative_fd_and_canonicalizes_state)
{
    auto const fd = int{-2};
    auto sut = sut_t{fd};

    EXPECT_EQ(fd, sut.release());

    EXPECT_EQ(sut_t::disarmed_fd, sut.get());
}

TEST_F(unique_fd_test_t, reset_preserves_negative_fd_as_disarmed)
{
    auto sut = sut_t{};
    auto const fd = int{-2};

    sut.reset(fd);

    EXPECT_FALSE(sut);
    EXPECT_EQ(fd, sut.get());
}

TEST_F(unique_fd_test_t, reset_without_closer_preserves_current_closer)
{
    auto const old_fd = int{17};
    auto const new_fd = int{23};
    auto sut = sut_t{old_fd, closer_t{&mock_closer}};

    expect_close(old_fd);

    sut.reset(new_fd);

    EXPECT_EQ(new_fd, sut.get());
    expect_close(new_fd);
}

TEST_F(unique_fd_test_t, reset_to_disarmed_preserves_closer_for_later_adoption)
{
    auto const old_fd = int{17};
    auto const new_fd = int{23};
    auto sut = sut_t{old_fd, closer_t{&mock_closer}};

    auto const seq = InSequence{};
    expect_close(old_fd);

    sut.reset();

    EXPECT_FALSE(sut);

    sut.reset(new_fd);

    EXPECT_EQ(new_fd, sut.get());

    expect_close(new_fd);
}

//
// disarmed
//

struct unique_fd_test_disarmed_t : unique_fd_test_t
{
    sut_t sut{};
};

TEST_F(unique_fd_test_disarmed_t, operator_bool_is_false)
{
    EXPECT_FALSE(static_cast<bool>(sut));
}

TEST_F(unique_fd_test_disarmed_t, get_returns_disarmed_fd)
{
    EXPECT_EQ(sut_t::disarmed_fd, sut.get());
}

TEST_F(unique_fd_test_disarmed_t, dereference_returns_disarmed_fd)
{
    EXPECT_EQ(sut_t::disarmed_fd, *sut);
}

TEST_F(unique_fd_test_disarmed_t, move_construction_from_disarmed_remains_disarmed)
{
    auto const dst = sut_t{std::move(sut)};

    EXPECT_EQ(sut_t::disarmed_fd, dst.get());
    EXPECT_EQ(sut_t::disarmed_fd, sut.get());
}

TEST_F(unique_fd_test_disarmed_t, self_move_assignment_is_noop)
{
    auto& actual = (sut = std::move(sut));

    EXPECT_EQ(&sut, &actual);
    EXPECT_EQ(sut_t::disarmed_fd, sut.get());
}

TEST_F(unique_fd_test_disarmed_t, assign_armed_adopts_fd)
{
    auto const new_fd = int{17};

    auto& actual = (sut = sut_t{new_fd, closer_t{&new_mock_closer}});

    EXPECT_EQ(&sut, &actual);
    EXPECT_EQ(new_fd, sut.get());

    expect_close(new_fd, new_mock_closer);
}

TEST_F(unique_fd_test_disarmed_t, move_assignment_from_disarmed_remains_disarmed)
{
    auto const new_fd = int{-2};

    auto& actual = (sut = sut_t{new_fd});

    EXPECT_EQ(&sut, &actual);
    EXPECT_FALSE(sut);
    EXPECT_EQ(new_fd, sut.get());
}

TEST_F(unique_fd_test_disarmed_t, move_assignment_from_armed_adopts_ownership)
{
    auto const new_fd = int{0};

    auto& actual = (sut = sut_t{new_fd, closer_t{&new_mock_closer}});

    EXPECT_EQ(&sut, &actual);
    EXPECT_TRUE(sut);
    EXPECT_EQ(new_fd, sut.get());

    expect_close(new_fd, new_mock_closer);
}

TEST_F(unique_fd_test_disarmed_t, release_returns_fd)
{
    EXPECT_EQ(sut_t::disarmed_fd, sut.release());
}

TEST_F(unique_fd_test_disarmed_t, release_leaves_canonical_disarmed_state)
{
    static_cast<void>(sut.release());

    EXPECT_EQ(sut_t::disarmed_fd, sut.get());
}

TEST_F(unique_fd_test_disarmed_t, reset_to_disarmed_closes_neither)
{
    auto const new_fd = int{-2};

    sut.reset(new_fd);

    EXPECT_FALSE(sut);
    EXPECT_EQ(new_fd, sut.get());
}

TEST_F(unique_fd_test_disarmed_t, reset_to_armed_adopts_ownership)
{
    auto const new_fd = int{0};

    sut.reset(new_fd, closer_t{&new_mock_closer});

    EXPECT_TRUE(sut);
    EXPECT_EQ(new_fd, sut.get());

    expect_close(new_fd, new_mock_closer);
}

TEST_F(unique_fd_test_disarmed_t, reset_to_same_does_not_close)
{
    sut.reset(sut.get());
}

//
// armed
//

struct unique_fd_test_armed_t : unique_fd_test_t
{
    int const fd = 0;
    sut_t sut{fd, closer_t{&mock_closer}};
};

TEST_F(unique_fd_test_armed_t, operator_bool_is_true)
{
    EXPECT_TRUE(static_cast<bool>(sut));

    expect_close(fd);
}

TEST_F(unique_fd_test_armed_t, get_is_fd)
{
    EXPECT_EQ(fd, sut.get());

    expect_close(fd);
}

TEST_F(unique_fd_test_armed_t, dereferences_to_fd)
{
    EXPECT_EQ(fd, *sut);

    expect_close(fd);
}

TEST_F(unique_fd_test_armed_t, move_construct_transfers_fd_and_ownership)
{
    auto const dst = sut_t{std::move(sut)};

    EXPECT_EQ(fd, dst.get());
    EXPECT_EQ(sut_t::disarmed_fd, sut.get());

    expect_close(fd);
}

TEST_F(unique_fd_test_armed_t, move_assignment_from_disarmed_closes_owned_fd)
{
    expect_close(fd);

    auto& actual = (sut = sut_t{sut_t::disarmed_fd});

    EXPECT_EQ(&sut, &actual);
}

TEST_F(unique_fd_test_armed_t, move_assignment_closes_old_and_adopts_new_fd)
{
    auto const new_fd = int{1};
    expect_close(fd);

    auto& actual = (sut = sut_t{new_fd, closer_t{&new_mock_closer}});

    EXPECT_EQ(&sut, &actual);

    expect_close(new_fd, new_mock_closer);
}

TEST_F(unique_fd_test_armed_t, self_assign_noop)
{
    auto& actual = (sut = std::move(sut));

    EXPECT_EQ(&sut, &actual);
    EXPECT_EQ(fd, sut.get());

    expect_close(fd);
}

TEST_F(unique_fd_test_armed_t, release_disarms_and_returns_fd)
{
    EXPECT_EQ(fd, sut.release());

    EXPECT_FALSE(sut);
}

TEST_F(unique_fd_test_armed_t, release_preserves_closer_for_later_adoption)
{
    auto const new_fd = int{1};

    EXPECT_EQ(fd, sut.release());

    sut.reset(new_fd);

    EXPECT_EQ(new_fd, sut.get());

    expect_close(new_fd);
}

TEST_F(unique_fd_test_armed_t, release_leaves_canonical_disarmed_state)
{
    static_cast<void>(sut.release());

    EXPECT_EQ(sut_t::disarmed_fd, sut.get());
}

TEST_F(unique_fd_test_armed_t, reset_to_disarmed_closes_old)
{
    auto new_fd = sut_t::disarmed_fd;
    expect_close(fd);

    sut.reset(new_fd);

    EXPECT_FALSE(sut);
    EXPECT_EQ(new_fd, sut.get());
}

TEST_F(unique_fd_test_armed_t, reset_closes_old_and_adopts_new_fd)
{
    auto const new_fd = int{1};
    expect_close(fd);

    sut.reset(new_fd, closer_t{&new_mock_closer});

    EXPECT_TRUE(sut);
    EXPECT_EQ(new_fd, sut.get());

    expect_close(new_fd, new_mock_closer);
}

// this test documents behavior, but doing this in prod would double close
TEST_F(unique_fd_test_armed_t, reset_to_same_fd_invokes_old_and_new_closers)
{
    auto const seq = InSequence{};
    expect_close(fd);

    sut.reset(sut.get(), closer_t{&new_mock_closer});

    EXPECT_EQ(fd, sut.get());

    expect_close(fd, new_mock_closer);
}

TEST_F(unique_fd_test_armed_t, reset_with_closer_closes_old_with_old_closer_and_adopts_new_closer)
{
    auto const new_fd = int{1};

    auto const seq = InSequence{};
    expect_close(fd);
    expect_close(new_fd, new_mock_closer);

    sut.reset(new_fd, closer_t{&new_mock_closer});

    EXPECT_EQ(new_fd, sut.get());
}

} // namespace
} // namespace crv
