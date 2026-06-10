// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "command_adapter.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::command::qt {
namespace {

struct command_adapter_test_t : Test
{
    template <typename command_t> struct mock_command_t
    {
        virtual ~mock_command_t() = default;

        MOCK_METHOD(id_t, id, (), (noexcept));
        MOCK_METHOD(void, apply, ());
        MOCK_METHOD(void, undo, ());
        MOCK_METHOD(idle_duration_t, idle_duration, (), (const, noexcept));
        MOCK_METHOD(bool, try_merge, (command_t const& command));
    };

    idle_clock_t::time_point const idle_time_point{idle_clock_t::duration{1234}};
};

//
// not mergable
//

struct command_adapter_test_not_mergeable_t : command_adapter_test_t
{
    struct command_t;
    using mock_command_t = mock_command_t<command_t>;

    struct command_t
    {
        mock_command_t* mock = nullptr;

        auto id() const noexcept -> id_t { return mock->id(); }
        auto apply() -> void { mock->apply(); }
        auto undo() -> void { mock->undo(); }
    };

    StrictMock<mock_command_t> mock_command;

    using sut_t = command_adapter_t<command_t>;
    sut_t sut{command_t{.mock = &mock_command}, idle_time_point};
};

TEST_F(command_adapter_test_not_mergeable_t, id)
{
    auto const expected = 3;

    EXPECT_CALL(mock_command, id()).WillOnce(Return(static_cast<id_t>(expected)));

    auto const actual = sut.id();

    EXPECT_EQ(expected, actual);
}

TEST_F(command_adapter_test_not_mergeable_t, redo)
{
    EXPECT_CALL(mock_command, apply());
    sut.redo();
}

TEST_F(command_adapter_test_not_mergeable_t, undo)
{
    EXPECT_CALL(mock_command, undo());
    sut.undo();
}

TEST_F(command_adapter_test_not_mergeable_t, merge_with_nullptr)
{
    EXPECT_FALSE(sut.mergeWith(nullptr));
}

TEST_F(command_adapter_test_not_mergeable_t, merge_with_self)
{
    EXPECT_FALSE(sut.mergeWith(&sut));
}

TEST_F(command_adapter_test_not_mergeable_t, merge_with_other)
{
    auto const other_sut = sut_t{command_t{.mock = nullptr}, idle_time_point};
    EXPECT_FALSE(sut.mergeWith(&sut));
}

//
// mergable
//

struct command_adapter_test_mergeable_t : command_adapter_test_t
{
    struct command_t;
    using sut_t = command_adapter_t<command_t>;
    using mock_command_t = mock_command_t<command_t>;

    struct command_t
    {
        mock_command_t* mock = nullptr;

        auto id() const noexcept -> id_t { return mock->id(); }
        auto apply() -> void { mock->apply(); }
        auto undo() -> void { mock->undo(); }
        auto idle_duration() const noexcept -> idle_duration_t { return mock->idle_duration(); }
        auto try_merge(command_t const& command) -> bool { return mock->try_merge(command); }
    };

    StrictMock<mock_command_t> mock_command;
    sut_t sut{command_t{.mock = &mock_command}, idle_time_point};

    StrictMock<mock_command_t> other_mock_command;

    auto expect_try_merge(bool expected_result, mock_command_t& mock) -> void
    {
        EXPECT_CALL(mock_command, try_merge(testing::Field(&command_t::mock, &mock))).WillOnce(Return(expected_result));
    }
};

TEST_F(command_adapter_test_mergeable_t, id)
{
    auto const expected = 3;

    EXPECT_CALL(mock_command, id()).WillOnce(Return(static_cast<id_t>(expected)));

    auto const actual = sut.id();

    EXPECT_EQ(expected, actual);
}

TEST_F(command_adapter_test_mergeable_t, redo)
{
    EXPECT_CALL(mock_command, apply());
    sut.redo();
}

TEST_F(command_adapter_test_mergeable_t, undo)
{
    EXPECT_CALL(mock_command, undo());
    sut.undo();
}

TEST_F(command_adapter_test_mergeable_t, merge_with_nullptr)
{
    EXPECT_FALSE(sut.mergeWith(nullptr));
}

TEST_F(command_adapter_test_mergeable_t, merge_with_other_type)
{
    struct other_command_t;
    using other_command_adapter_t = command_adapter_t<other_command_t>;
    using other_mock_command_t = command_adapter_test_t::mock_command_t<other_command_t>;

    struct other_command_t
    {
        other_mock_command_t* mock = nullptr;

        auto id() const noexcept -> id_t { return mock->id(); }
        auto apply() -> void { mock->apply(); }
        auto undo() -> void { mock->undo(); }
        auto idle_duration() const noexcept -> idle_duration_t { return mock->idle_duration(); }
        auto try_merge(other_command_t const& other_command) -> bool { return mock->try_merge(other_command); }
    };
    auto other_mock_command = StrictMock<other_mock_command_t>{};
    auto other_command_adapter = other_command_adapter_t{other_command_t{.mock = &other_mock_command}, idle_time_point};

    EXPECT_FALSE(sut.mergeWith(&other_command_adapter));
}

TEST_F(command_adapter_test_mergeable_t, merge_with_self_failure_try_merge)
{
    EXPECT_CALL(mock_command, idle_duration()).WillOnce(Return(idle_duration_t{0}));
    expect_try_merge(false, mock_command);
    EXPECT_FALSE(sut.mergeWith(&sut));
}

TEST_F(command_adapter_test_mergeable_t, merge_with_self_success)
{
    EXPECT_CALL(mock_command, idle_duration()).WillOnce(Return(idle_duration_t{0}));
    expect_try_merge(true, mock_command);
    EXPECT_TRUE(sut.mergeWith(&sut));
}

TEST_F(command_adapter_test_mergeable_t, merge_with_other_failure_idle_duration)
{
    auto const other_sut = sut_t{command_t{.mock = &other_mock_command}, idle_time_point + idle_duration_t{1}};
    EXPECT_CALL(mock_command, idle_duration()).WillOnce(Return(idle_duration_t{0}));
    EXPECT_FALSE(sut.mergeWith(&other_sut));
}

TEST_F(command_adapter_test_mergeable_t, merge_with_other_failure_try_merge)
{
    auto const other_sut = sut_t{command_t{.mock = &other_mock_command}, idle_time_point + idle_duration_t{1}};
    EXPECT_CALL(mock_command, idle_duration()).WillOnce(Return(idle_duration_t{2}));
    expect_try_merge(false, other_mock_command);
    EXPECT_FALSE(sut.mergeWith(&other_sut));
}

TEST_F(command_adapter_test_mergeable_t, merge_with_other_success)
{
    auto const other_sut = sut_t{command_t{.mock = &other_mock_command}, idle_time_point};
    EXPECT_CALL(mock_command, idle_duration()).WillOnce(Return(idle_duration_t{0}));
    expect_try_merge(true, other_mock_command);
    EXPECT_TRUE(sut.mergeWith(&other_sut));
}

TEST_F(command_adapter_test_mergeable_t, merge_updates_idle_time_point)
{
    auto const expiration_duration = idle_duration_t{10};

    EXPECT_CALL(mock_command, idle_duration()).WillRepeatedly(Return(expiration_duration));
    EXPECT_CALL(mock_command, try_merge(_)).WillRepeatedly(Return(true));

    // first merge updates timer from 0 to 8
    auto const intermediate_time = idle_time_point + idle_duration_t{8};
    auto const intermediate_sut = sut_t{command_t{.mock = &other_mock_command}, intermediate_time};
    EXPECT_TRUE(sut.mergeWith(&intermediate_sut));

    // second merge relies on timer starting at 8
    // duration is 4, which is less than expiration of 10
    // if previous merge didn't update timer, duration would be 12 - 0 = 12, which exceeds expiration
    auto const final_time = intermediate_time + idle_duration_t{4};
    auto const final_sut = sut_t{command_t{.mock = &other_mock_command}, final_time};
    EXPECT_TRUE(sut.mergeWith(&final_sut));
}

} // namespace
} // namespace crv::command::qt
