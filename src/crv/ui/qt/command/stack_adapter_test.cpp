// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "stack_adapter.hpp"
#include <crv/test/test.hpp>
#include <crv/ui/command/command.hpp>
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

        MOCK_METHOD(void, apply, (std::string const& param_string, int_t param_int), (const, noexcept));
        MOCK_METHOD(void, undo, (), (const, noexcept));
    };
    StrictMock<mock_command_t> mock_command;

    struct command_t
    {
        mock_command_t* mock = nullptr;

        id_t id_;

        // Parameters passed during emplace are not directly accessible after construction. They are smuggled during
        // into the mock during apply to make sure they are forwarded correctly.
        std::string param_string;
        int_t param_int;

        auto id() const noexcept -> id_t { return id_; }
        auto apply() noexcept -> void { mock->apply(param_string, param_int); }
        auto undo() noexcept -> void { mock->undo(); }
    };

    using sut_t = stack_adapter_t<mock_impl_t>;
    sut_t sut{mock_impl};

    id_t const id = static_cast<id_t>(3);
    std::string const param_string{"param_string"};
    int_t const param_int{5};

    auto emplace() -> std::unique_ptr<command_adapter_t<command_t>>
    {
        auto command_adapter = std::unique_ptr<command_adapter_t<command_t>>{};
        EXPECT_CALL(mock_impl, push(_)).WillOnce(Invoke([&](QUndoCommand* bare_command) {
            command_adapter.reset(dynamic_cast<command_adapter_t<command_t>*>(bare_command));
        }));
        sut.template emplace<command_t>(&mock_command, id, param_string, param_int);
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

    EXPECT_CALL(mock_command, apply(param_string, param_int));
    command_adapter->redo();
}

TEST_F(command_stack_adapter_test_t, undo)
{
    auto command_adapter = emplace();
    ASSERT_NE(command_adapter, nullptr);

    EXPECT_CALL(mock_command, undo());
    command_adapter->undo();
}

} // namespace
} // namespace crv::command::qt
