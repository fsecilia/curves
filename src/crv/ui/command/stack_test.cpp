// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "stack.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::command {
namespace {

struct command_stack_test_t : Test
{
    struct command_t
    {
        int_t id = 0;
        constexpr auto operator==(command_t const&) const noexcept -> bool = default;
    };

    struct mock_adapter_t
    {
        virtual ~mock_adapter_t() = default;

        MOCK_METHOD(void, push, (command_t command));
        MOCK_METHOD(void, undo, ());
        MOCK_METHOD(void, redo, ());
    };
    StrictMock<mock_adapter_t> mock_adapter;

    struct adapter_t
    {
        mock_adapter_t* mock = nullptr;

        auto push(command_t command) -> void { mock->push(std::move(command)); }
        auto undo() -> void { mock->undo(); }
        auto redo() -> void { mock->redo(); }
    };

    using sut_t = stack_t<adapter_t>;
    sut_t sut{adapter_t{&mock_adapter}};
};

TEST_F(command_stack_test_t, push)
{
    command_t expected_command{.id = 5};
    EXPECT_CALL(mock_adapter, push(expected_command));

    sut.push(expected_command);
}

TEST_F(command_stack_test_t, undo)
{
    EXPECT_CALL(mock_adapter, undo());
    sut.undo();
}

TEST_F(command_stack_test_t, redo)
{
    EXPECT_CALL(mock_adapter, redo());
    sut.redo();
}

} // namespace
} // namespace crv::command
