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
    struct mock_command_t
    {
        virtual ~mock_command_t() = default;

        MOCK_METHOD(id_t, id, (), (noexcept));
        MOCK_METHOD(void, apply, (), (noexcept));
        MOCK_METHOD(void, undo, (), (noexcept));
    };
    StrictMock<mock_command_t> mock_command;

    struct command_t
    {
        mock_command_t* mock = nullptr;

        auto id() const noexcept -> id_t { return mock->id(); }
        auto apply() noexcept -> void { mock->apply(); }
        auto undo() noexcept -> void { mock->undo(); }
    };

    using sut_t = command_adapter_t<command_t>;
    sut_t sut{command_t{.mock = &mock_command}};
};

TEST_F(command_adapter_test_t, id)
{
    auto const expected = static_cast<id_t>(3);

    EXPECT_CALL(mock_command, id()).WillOnce(Return(expected));

    auto const actual = sut.id();

    EXPECT_EQ(expected, actual);
}

TEST_F(command_adapter_test_t, redo)
{
    EXPECT_CALL(mock_command, apply());
    sut.redo();
}

TEST_F(command_adapter_test_t, undo)
{
    EXPECT_CALL(mock_command, undo());
    sut.undo();
}

} // namespace
} // namespace crv::command::qt
