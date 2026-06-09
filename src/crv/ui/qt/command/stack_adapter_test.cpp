// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "stack_adapter.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::command::qt {
namespace {

struct command_stack_adapter_test_t : Test
{
    struct mock_impl_t
    {
        virtual ~mock_impl_t() = default;

        MOCK_METHOD(void, push, (QUndoCommand*), ());
        MOCK_METHOD(void, undo, (), ());
        MOCK_METHOD(void, redo, (), ());
    };
    StrictMock<mock_impl_t> mock_impl;

    struct mock_command_t
    {
        virtual ~mock_command_t() = default;

        MOCK_METHOD(void, apply, (int_t id, std::string const& name), (const, noexcept));
        MOCK_METHOD(void, undo, (int_t id, std::string const& name), (const, noexcept));
    };
    StrictMock<mock_command_t> mock_command;

    struct command_t
    {
        mock_command_t* mock = nullptr;
        int_t id;
        std::string name;

        auto apply() noexcept -> void { mock->apply(id, name); }
        auto undo() noexcept -> void { mock->undo(id, name); }
    };

    using sut_t = stack_adapter_t<mock_impl_t>;
    sut_t sut{mock_impl};

    int_t const id = 3;
    std::string const name{"name"};

    auto emplace() -> std::unique_ptr<command_adapter_t<command_t>>
    {
        auto command_adapter = std::unique_ptr<command_adapter_t<command_t>>{};
        EXPECT_CALL(mock_impl, push(_)).WillOnce(Invoke([&](QUndoCommand* bare_command) {
            command_adapter.reset(dynamic_cast<command_adapter_t<command_t>*>(bare_command));
        }));
        sut.template emplace<command_t>(&mock_command, id, name);
        return command_adapter;
    }
};

TEST_F(command_stack_adapter_test_t, emplace_pushes_correctly_allocated_command)
{
    auto command_adapter = emplace();
    EXPECT_NE(command_adapter, nullptr);
}

TEST_F(command_stack_adapter_test_t, redo)
{
    auto command_adapter = emplace();
    ASSERT_NE(command_adapter, nullptr);

    EXPECT_CALL(mock_command, apply(id, name));
    command_adapter->redo();
}

TEST_F(command_stack_adapter_test_t, undo)
{
    auto command_adapter = emplace();
    ASSERT_NE(command_adapter, nullptr);

    EXPECT_CALL(mock_command, undo(id, name));
    command_adapter->undo();
}

} // namespace
} // namespace crv::command::qt
