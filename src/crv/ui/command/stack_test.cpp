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
    {};

    struct mock_adapter_t
    {
        virtual ~mock_adapter_t() = default;

        MOCK_METHOD(void, emplace, (int_t id, std::string const& name));
        MOCK_METHOD(void, undo, ());
        MOCK_METHOD(void, redo, ());
    };
    StrictMock<mock_adapter_t> mock_adapter;

    struct adapter_t
    {
        mock_adapter_t* mock = nullptr;

        template <typename command_t, typename... args_t> auto emplace(args_t&&... args) -> void
        {
            mock->emplace(std::forward<args_t>(args)...);
        }
        auto undo() -> void { mock->undo(); }
        auto redo() -> void { mock->redo(); }
    };

    int_t const id = 3;
    std::string name = "name";

    using sut_t = stack_t<adapter_t>;
    sut_t sut{adapter_t{&mock_adapter}};
};

TEST_F(command_stack_test_t, push)
{
    EXPECT_CALL(mock_adapter, emplace(id, name));
    sut.template emplace<command_t>(id, name);
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
