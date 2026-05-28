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
    value_t const requested_value = 7;
    value_t const constrained_value = 5;

    using sut_t = param_t<value_t, constraint_t>;

    struct mock_visitor_t
    {
        virtual ~mock_visitor_t() = default;

        auto visit(auto const& param) const noexcept -> void { visit_const(param); }
        auto visit(auto& param) const noexcept -> void { visit_mutable(param); }

        MOCK_METHOD(void, visit_const, (sut_t const&), (const, noexcept));
        MOCK_METHOD(void, visit_mutable, (sut_t&), (const, noexcept));
    };
    StrictMock<mock_visitor_t> mock_visitor;
};

TEST_F(reflection_param_test_t, ctor_applies_constraint_value_unconstrained)
{
    EXPECT_CALL(mock_constraint, call(requested_value)).WillOnce(Return(requested_value));
    auto const sut = sut_t{name, requested_value, constraint_t{&mock_constraint}};

    EXPECT_EQ(requested_value, sut.value());
}

TEST_F(reflection_param_test_t, ctor_applies_constraint_value_constrained)
{
    EXPECT_CALL(mock_constraint, call(requested_value)).WillOnce(Return(constrained_value));
    auto const sut = sut_t{name, requested_value, constraint_t{&mock_constraint}};

    EXPECT_EQ(constrained_value, sut.value());
}

// --------------------------------------------------------------------------------------------------------------------

struct reflection_param_test_constructed_t : reflection_param_test_t
{
    value_t const value = 3;

    auto create_constraint() -> constraint_t
    {
        EXPECT_CALL(mock_constraint, call(value)).WillOnce(Return(value));
        return constraint_t{&mock_constraint};
    }

    sut_t sut{name, value, create_constraint()};
};

TEST_F(reflection_param_test_constructed_t, name)
{
    EXPECT_EQ(name, sut.name());
}

TEST_F(reflection_param_test_constructed_t, value)
{
    EXPECT_EQ(value, sut.value());
}

TEST_F(reflection_param_test_constructed_t, constraint)
{
    EXPECT_EQ(&mock_constraint, sut.constraint().mock);
}

TEST_F(reflection_param_test_constructed_t, value_returns_false_when_unconstrained)
{
    EXPECT_CALL(mock_constraint, call(requested_value)).WillOnce(Return(requested_value));

    EXPECT_FALSE(sut.value(requested_value));

    EXPECT_EQ(requested_value, sut.value());
}

TEST_F(reflection_param_test_constructed_t, value_returns_true_when_constrained)
{
    EXPECT_CALL(mock_constraint, call(requested_value)).WillOnce(Return(constrained_value));

    EXPECT_TRUE(sut.value(requested_value));

    EXPECT_EQ(constrained_value, sut.value());
}

TEST_F(reflection_param_test_constructed_t, const_reflect_const_visitor_passes_const_self_returns_const_visitor)
{
    EXPECT_CALL(mock_visitor, visit_const(Ref(sut)));

    auto const& actual = static_cast<sut_t const&>(sut).reflect(static_cast<mock_visitor_t const&>(mock_visitor));

    EXPECT_EQ(&mock_visitor, &actual);
}

TEST_F(reflection_param_test_constructed_t, const_reflect_mutable_visitor_passes_const_self_returns_mutable_visitor)
{
    EXPECT_CALL(mock_visitor, visit_const(Ref(sut)));

    auto& actual = static_cast<sut_t const&>(sut).reflect(mock_visitor);

    EXPECT_EQ(&mock_visitor, &actual);
}

TEST_F(reflection_param_test_constructed_t, mutable_reflect_const_visitor_passes_mutable_self_returns_const_visitor)
{
    EXPECT_CALL(mock_visitor, visit_mutable(Ref(sut)));

    auto const& actual = sut.reflect(static_cast<mock_visitor_t const&>(mock_visitor));

    EXPECT_EQ(&mock_visitor, &actual);
}

TEST_F(reflection_param_test_constructed_t, mutable_reflect_mutable_visitor_passes_mutable_self_returns_mutable_visitor)
{
    EXPECT_CALL(mock_visitor, visit_mutable(Ref(sut)));

    auto& actual = sut.reflect(mock_visitor);

    EXPECT_EQ(&mock_visitor, &actual);
}

} // namespace
} // namespace crv::reflection
