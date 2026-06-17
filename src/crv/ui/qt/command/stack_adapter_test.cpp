// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "stack_adapter.hpp"
#include <crv/test/test.hpp>
#include <crv/ui/command/command.hpp>
#include <crv/ui/qt/test_app.hpp>
#include <QtTest/QSignalSpy>
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
        sut.template emplace<command_t>(true, &mock_command, id, param_string, param_int);
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

//
// new_stack_adapter_t
//

struct command_new_stack_adapter_test_t : Test
{
    test_app_t test_app;

    int_t command_execute_count = 0;
    struct command_t : public command_i
    {
        int_t& execute_count;

        explicit command_t(int_t& count) : execute_count{count} {}

        auto undo() -> void override { --execute_count; }
        auto redo() -> void override { ++execute_count; }
        [[nodiscard]] auto merge_timeout() const noexcept -> duration_t override { return duration_t::zero(); }
        auto try_merge(command_i&&) noexcept -> bool override { return false; }
    };

    using sut_t = new_stack_adapter_t;

    sut_t::stack_t stack;
    sut_t adapter{stack};

    QSignalSpy spy_can_undo{&adapter, &new_stack_adapter_t::canUndoChanged};
    QSignalSpy spy_can_redo{&adapter, &new_stack_adapter_t::canRedoChanged};
    QSignalSpy spy_undo_applied{&adapter, &new_stack_adapter_t::undoApplied};
    QSignalSpy spy_redo_applied{&adapter, &new_stack_adapter_t::redoApplied};

    command_new_stack_adapter_test_t() { stack.observer(&adapter); }
};

TEST_F(command_new_stack_adapter_test_t, initial_state_is_empty)
{
    EXPECT_FALSE(adapter.canUndo());
    EXPECT_FALSE(adapter.canRedo());
    EXPECT_TRUE(spy_can_undo.isEmpty());
    EXPECT_TRUE(spy_can_redo.isEmpty());
}

TEST_F(command_new_stack_adapter_test_t, push_emits_can_undo_changed)
{
    stack.emplace_now<command_t>(command_execute_count);

    EXPECT_TRUE(adapter.canUndo());
    EXPECT_FALSE(adapter.canRedo());

    ASSERT_EQ(spy_can_undo.count(), 1);
    EXPECT_TRUE(spy_can_undo.takeFirst().at(0).toBool());

    // redo state doesn't change from its initial state
    EXPECT_TRUE(spy_can_redo.isEmpty());
}

TEST_F(command_new_stack_adapter_test_t, undo_emits_applied_and_updates_states)
{
    stack.emplace_now<command_t>(command_execute_count);
    spy_can_undo.clear();

    adapter.undo();

    EXPECT_EQ(command_execute_count, 0);
    EXPECT_EQ(spy_undo_applied.count(), 1);

    ASSERT_EQ(spy_can_undo.count(), 1);
    EXPECT_FALSE(spy_can_undo.takeFirst().at(0).toBool());

    ASSERT_EQ(spy_can_redo.count(), 1);
    EXPECT_TRUE(spy_can_redo.takeFirst().at(0).toBool());
}

TEST_F(command_new_stack_adapter_test_t, redo_emits_applied_and_updates_states)
{
    stack.emplace_now<command_t>(command_execute_count);
    adapter.undo();

    spy_can_undo.clear();
    spy_can_redo.clear();

    adapter.redo();

    EXPECT_EQ(command_execute_count, 1);
    EXPECT_EQ(spy_redo_applied.count(), 1);

    ASSERT_EQ(spy_can_redo.count(), 1);
    EXPECT_FALSE(spy_can_redo.takeFirst().at(0).toBool());

    ASSERT_EQ(spy_can_undo.count(), 1);
    EXPECT_TRUE(spy_can_undo.takeFirst().at(0).toBool());
}

TEST_F(command_new_stack_adapter_test_t, clear_resets_availability_flags)
{
    stack.emplace_now<command_t>(command_execute_count);
    spy_can_undo.clear();

    adapter.clear();

    EXPECT_FALSE(adapter.canUndo());
    EXPECT_FALSE(adapter.canRedo());

    ASSERT_EQ(spy_can_undo.count(), 1);
    EXPECT_FALSE(spy_can_undo.takeFirst().at(0).toBool());
}

} // namespace
} // namespace crv::command::qt
