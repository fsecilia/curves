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

    struct mock_inspector_t
    {
        virtual ~mock_inspector_t() = default;

        auto inspect(auto const& param) const noexcept -> void { inspect_const(param); }
        auto inspect(auto& param) const noexcept -> void { inspect_mutable(param); }

        MOCK_METHOD(void, inspect_const, (sut_t const&), (const, noexcept));
        MOCK_METHOD(void, inspect_mutable, (sut_t&), (const, noexcept));
    };
    StrictMock<mock_inspector_t> mock_inspector;
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

TEST_F(reflection_param_test_constructed_t, const_reflect_const_inspector_passes_const_self_returns_const_inspector)
{
    EXPECT_CALL(mock_inspector, inspect_const(Ref(sut)));

    auto const& actual = static_cast<sut_t const&>(sut).reflect(static_cast<mock_inspector_t const&>(mock_inspector));

    EXPECT_EQ(&mock_inspector, &actual);
}

TEST_F(reflection_param_test_constructed_t, const_reflect_mutable_inspector_passes_const_self_returns_mutable_inspector)
{
    EXPECT_CALL(mock_inspector, inspect_const(Ref(sut)));

    auto& actual = static_cast<sut_t const&>(sut).reflect(mock_inspector);

    EXPECT_EQ(&mock_inspector, &actual);
}

TEST_F(reflection_param_test_constructed_t, mutable_reflect_const_inspector_passes_mutable_self_returns_const_inspector)
{
    EXPECT_CALL(mock_inspector, inspect_mutable(Ref(sut)));

    auto const& actual = sut.reflect(static_cast<mock_inspector_t const&>(mock_inspector));

    EXPECT_EQ(&mock_inspector, &actual);
}

TEST_F(reflection_param_test_constructed_t,
    mutable_reflect_mutable_inspector_passes_mutable_self_returns_mutable_inspector)
{
    EXPECT_CALL(mock_inspector, inspect_mutable(Ref(sut)));

    auto& actual = sut.reflect(mock_inspector);

    EXPECT_EQ(&mock_inspector, &actual);
}

} // namespace
} // namespace crv::reflection
