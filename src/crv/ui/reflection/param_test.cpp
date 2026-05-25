// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "param.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::reflection {
namespace {

struct reflection_param_test_t : Test
{
    using value_t = int_t;

    struct mock_constraint_t
    {
        virtual ~mock_constraint_t() = default;
        MOCK_METHOD(value_t, call, (value_t), (const, noexcept));
    };
    StrictMock<mock_constraint_t> mock_constraint;

    struct constraint_t
    {
        mock_constraint_t* mock = nullptr;
        auto operator()(value_t value) const noexcept -> value_t { return mock->call(value); }
    };

    std::string_view const name{"name"};
    value_t const value = 3;
    value_t new_value = 5;

    using sut_t = param_t<value_t, constraint_t>;
    sut_t sut{name, value, constraint_t{&mock_constraint}};

    struct mock_visitor_t
    {
        virtual ~mock_visitor_t() = default;

        auto operator()(auto const& param) const noexcept -> void { call_const(param); }
        auto operator()(auto& param) const noexcept -> void { call_mutable(param); }

        MOCK_METHOD(void, call_const, (sut_t const&), (const, noexcept));
        MOCK_METHOD(void, call_mutable, (sut_t&), (const, noexcept));
    };
    StrictMock<mock_visitor_t> mock_visitor;
};

TEST_F(reflection_param_test_t, name)
{
    EXPECT_EQ(name, sut.name());
}

TEST_F(reflection_param_test_t, value)
{
    EXPECT_EQ(value, sut.value());
}

TEST_F(reflection_param_test_t, set_value)
{
    sut.value(new_value);
    EXPECT_EQ(new_value, sut.value());
}

TEST_F(reflection_param_test_t, constraint)
{
    EXPECT_EQ(&mock_constraint, sut.constraint().mock);
}

TEST_F(reflection_param_test_t, constrain_no_change)
{
    EXPECT_CALL(mock_constraint, call(value)).WillOnce(Return(value));
    EXPECT_FALSE(sut.constrain());
}

TEST_F(reflection_param_test_t, constrain_with_change)
{
    EXPECT_CALL(mock_constraint, call(value)).WillOnce(Return(new_value));
    EXPECT_TRUE(sut.constrain());
    EXPECT_EQ(new_value, sut.value());
}

TEST_F(reflection_param_test_t, const_reflect_passes_const_self)
{
    EXPECT_CALL(mock_visitor, call_const(Ref(sut)));
    static_cast<sut_t const&>(sut).reflect(mock_visitor);
}

TEST_F(reflection_param_test_t, mutable_reflect_passes_mutable_self)
{
    EXPECT_CALL(mock_visitor, call_mutable(Ref(sut)));
    sut.reflect(mock_visitor);
}

} // namespace
} // namespace crv::reflection
