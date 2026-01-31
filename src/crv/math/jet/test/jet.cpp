// SPDX-License-Identifier: MIT
/**
    \file
    \copyright Copyright (C) 2026 Frank Secilia
*/

#include <crv/math/jet/jet.hpp>
#include <crv/test/test.hpp>
#include <ostream>

namespace crv {
namespace {

struct jet_test_t : Test
{
    using scalar_t = real_t;
    using sut_t    = jet_t<scalar_t>;

    static constexpr scalar_t f  = 37.2; // arbitrary
    static constexpr scalar_t df = 26.3; // arbitrary
    static constexpr sut_t    x{f, df};
};

// ====================================================================================================================
// Concepts
// ====================================================================================================================

struct jet_test_concepts_t : jet_test_t
{
    struct nonarithmetic_t;
    struct nonjet_t;
};

TEST_F(jet_test_concepts_t, arithmetic)
{
    static_assert(arithmetic<int_t>);
    static_assert(arithmetic<real_t>);

    static_assert(!arithmetic<nonarithmetic_t>);
}

TEST_F(jet_test_concepts_t, is_jet)
{
    static_assert(is_jet<sut_t>);
    static_assert(!is_jet<nonjet_t>);
}

TEST_F(jet_test_concepts_t, is_not_jet)
{
    static_assert(!is_not_jet<sut_t>);
    static_assert(is_not_jet<nonjet_t>);
}

// ====================================================================================================================
// Scalar Fallbacks
// ====================================================================================================================

struct jet_test_scalar_fallbacks_t : jet_test_t
{};

TEST_F(jet_test_scalar_fallbacks_t, primal)
{
    static_assert(f == primal(f));
    static_assert(-f == primal(-f));
    static_assert(df == primal(df));
}

TEST_F(jet_test_scalar_fallbacks_t, derivative)
{
    static_assert(scalar_t{} == derivative(f));
    static_assert(scalar_t{} == derivative(-f));
    static_assert(scalar_t{} == derivative(df));
}

// ====================================================================================================================
// Construction
// ====================================================================================================================

struct jet_test_construction_t : jet_test_t
{};

TEST_F(jet_test_construction_t, default_construction)
{
    constexpr auto sut = sut_t{};

    static_assert(sut.f == 0.0);
    static_assert(sut.df == 0.0);
}

TEST_F(jet_test_construction_t, scalar)
{
    constexpr auto sut = jet_t{f};

    static_assert(sut.f == f);
    static_assert(sut.df == 0.0);
}

TEST_F(jet_test_construction_t, nested_scalar)
{
    constexpr auto sut = jet_t<jet_t<double>>{x};

    static_assert(sut.f == x);
    static_assert(sut.df == jet_t{0.0});
}

TEST_F(jet_test_construction_t, pair)
{
    static_assert(x.f == f);
    static_assert(x.df == df);
}

TEST_F(jet_test_construction_t, broadcast)
{
    constexpr auto sut = jet_t<jet_t<double>>{f};

    static_assert(sut.f == jet_t{f});
    static_assert(sut.df == jet_t{0.0});
}

// ====================================================================================================================
// Conversion
// ====================================================================================================================

struct jet_test_conversion_t : jet_test_t
{
    static constexpr int_t f_int  = 7;
    static constexpr int_t df_int = 11;
};

TEST_F(jet_test_conversion_t, ctor)
{
    constexpr auto jet_int = jet_t{f_int, df_int};
    constexpr auto sut     = sut_t{jet_int};

    static_assert(primal(sut) == static_cast<scalar_t>(f_int));
    static_assert(derivative(sut) == static_cast<scalar_t>(df_int));
}

TEST_F(jet_test_conversion_t, assign)
{
    constexpr auto jet_int = jet_t{f_int, df_int};
    auto           sut     = sut_t{};

    sut = jet_int;

    EXPECT_DOUBLE_EQ(primal(sut), static_cast<scalar_t>(f_int));
    EXPECT_DOUBLE_EQ(derivative(sut), static_cast<scalar_t>(df_int));
}

TEST_F(jet_test_conversion_t, to_bool_true)
{
    static_assert(static_cast<bool>(sut_t{1.0, 0.0}));
    static_assert(static_cast<bool>(sut_t{-1.0, 0.0}));
    static_assert(static_cast<bool>(sut_t{0.001, 0.0}));
    static_assert(static_cast<bool>(sut_t{1.0, 999.0}));
}

TEST_F(jet_test_conversion_t, to_bool_false)
{
    static_assert(!static_cast<bool>(sut_t{0.0, 0.0}));
    static_assert(!static_cast<bool>(sut_t{0.0, 999.0}));
}

// ====================================================================================================================
// Comparison
// ====================================================================================================================

struct jet_test_comparison_t : jet_test_t
{};

TEST_F(jet_test_comparison_t, element_equality)
{
    // equal only if primal matches AND derivative is zero
    static_assert(jet_t{5.0, 0.0} == 5.0);
    static_assert(jet_t{5.0, 1.0} != 5.0);
    static_assert(jet_t{5.1, 0.0} != 5.0);
}

TEST_F(jet_test_comparison_t, element_ordering)
{
    static_assert(jet_t{3.0, 999.0} != 4.0);
    static_assert(jet_t{3.0, 999.0} < 4.0);

    static_assert(jet_t{5.0, 999.0} != 4.0);
    static_assert(jet_t{5.0, 999.0} > 4.0);

    static_assert(jet_t{4.0, 999.0} != 4.0);
    static_assert(jet_t{4.0, 999.0} <= 4.0);
    static_assert(jet_t{4.0, 999.0} >= 4.0);
}

TEST_F(jet_test_comparison_t, jet_equality)
{
    static_assert(jet_t{3.0, 2.0} == jet_t{3.0, 2.0});
    static_assert(jet_t{3.0, 2.0} != jet_t{3.0, 3.0});
    static_assert(jet_t{3.0, 2.0} != jet_t{4.0, 2.0});
}

TEST_F(jet_test_comparison_t, jet_ordering)
{
    static_assert(jet_t{3.0, 999.0} != jet_t{4.0, 0.0});
    static_assert(jet_t{3.0, 999.0} < jet_t{4.0, 0.0});

    static_assert(jet_t{5.0, 0.0} != jet_t{4.0, 999.0});
    static_assert(jet_t{5.0, 0.0} > jet_t{4.0, 999.0});

    static_assert(jet_t{4.0, 1.0} != jet_t{4.0, 2.0});
    static_assert(jet_t{4.0, 1.0} <= jet_t{4.0, 2.0});
    static_assert(jet_t{4.0, 2.0} >= jet_t{4.0, 1.0});
}

// ====================================================================================================================
// Accessors
// ====================================================================================================================

struct jet_test_accessors_t : jet_test_t
{};

TEST_F(jet_test_construction_t, primal)
{
    static_assert(x.f == primal(x));
}

TEST_F(jet_test_construction_t, derivative)
{
    static_assert(x.df == derivative(x));
}

// ====================================================================================================================
// Unary Arithmetic
// ====================================================================================================================

struct jet_test_unary_arithmetic_t : jet_test_t
{};

TEST_F(jet_test_unary_arithmetic_t, plus)
{
    constexpr auto expected = sut_t{x.f, x.df};
    constexpr auto actual   = +x;

    static_assert(expected == actual);
}

TEST_F(jet_test_unary_arithmetic_t, minus)
{
    constexpr auto expected = sut_t{-x.f, -x.df};
    constexpr auto actual   = -x;

    static_assert(expected == actual);
}

// ====================================================================================================================
// Scalar Arithmetic
// ====================================================================================================================

struct jet_scalar_op_vector_t
{
    using scalar_t = jet_test_t::scalar_t;
    using sut_t    = jet_test_t::sut_t;

    std::string name;
    sut_t       jet;
    scalar_t    scalar;
    sut_t       expected;

    friend auto operator<<(std::ostream& out, jet_scalar_op_vector_t const& src) -> std::ostream&
    {
        return out << src.jet << " ⨂ " << src.scalar << " = " << src.expected;
    }
};

struct jet_test_scalar_op_t : jet_test_t, WithParamInterface<jet_scalar_op_vector_t>
{
    sut_t          jet      = GetParam().jet;
    scalar_t const scalar   = GetParam().scalar;
    sut_t const&   expected = GetParam().expected;
};

// --------------------------------------------------------------------------------------------------------------------
// Addition
// --------------------------------------------------------------------------------------------------------------------

struct jet_test_scalar_addition_t : jet_test_scalar_op_t
{};

TEST_P(jet_test_scalar_addition_t, compound_assign)
{
    auto const& actual = jet += scalar;

    EXPECT_EQ(&jet, &actual);
    EXPECT_EQ(expected, actual);
}

TEST_P(jet_test_scalar_addition_t, jet_plus_scalar)
{
    auto const actual = jet + scalar;

    EXPECT_EQ(expected, actual);
}

TEST_P(jet_test_scalar_addition_t, scalar_plus_jet)
{
    auto const actual = scalar + jet;

    EXPECT_EQ(expected, actual);
}

// clang-format off
jet_scalar_op_vector_t const scalar_addition_vectors[] = {
    {"positve + positive", {3.0, 5.0}, 11, {14.0, 5.0}},
    {"negative + negative", {-3.0, -5.0}, -11, {-14.0, -5.0}},
    {"mixed signs", {3.0, -5.0}, -11, {-8.0, -5.0}},
    {"zeros", {0.0, 0.0}, 0.0, {0.0, 0.0}},
    {"mixed zeros", {5.0, 0.0}, 0.0, {5.0, 0.0}},
};
// clang-format on
INSTANTIATE_TEST_SUITE_P(addition, jet_test_scalar_addition_t, ValuesIn(scalar_addition_vectors),
                         test_name_generator_t<jet_scalar_op_vector_t>{});

// --------------------------------------------------------------------------------------------------------------------
// Subtraction
// --------------------------------------------------------------------------------------------------------------------

struct jet_test_scalar_subtraction_t : jet_test_scalar_op_t
{};

TEST_P(jet_test_scalar_subtraction_t, compound_assign)
{
    auto const& actual = jet -= scalar;

    EXPECT_EQ(&jet, &actual);
    EXPECT_EQ(expected, actual);
}

TEST_P(jet_test_scalar_subtraction_t, jet_minus_scalar)
{
    auto const actual = jet - scalar;

    EXPECT_EQ(expected, actual);
}

TEST_P(jet_test_scalar_subtraction_t, scalar_minus_jet)
{
    auto const actual = scalar - jet;

    EXPECT_EQ(-expected, actual);
}

// clang-format off
jet_scalar_op_vector_t const scalar_subtraction_vectors[] = {
    {"positve - positive", {3.0, 5.0}, 11, {-8.0, 5.0}},
    {"negative - negative", {-3.0, -5.0}, -11, {8.0, -5.0}},
    {"mixed signs", {3.0, -5.0}, -11, {14.0, -5.0}},
    {"zeros", {0.0, 0.0}, 0.0, {0.0, 0.0}},
    {"mixed zeros", {5.0, 0.0}, 0.0, {5.0, 0.0}},
};
// clang-format on
INSTANTIATE_TEST_SUITE_P(subtraction, jet_test_scalar_subtraction_t, ValuesIn(scalar_subtraction_vectors),
                         test_name_generator_t<jet_scalar_op_vector_t>{});

// ====================================================================================================================
// Vector Arithmetic
// ====================================================================================================================

struct jet_vector_op_vector_t
{
    using sut_t = jet_test_t::sut_t;

    std::string name;
    sut_t       lhs;
    sut_t       rhs;
    sut_t       expected;

    friend auto operator<<(std::ostream& out, jet_vector_op_vector_t const& src) -> std::ostream&
    {
        return out << src.lhs << " ⨂ " << src.rhs << " = " << src.expected;
    }
};

struct jet_test_vector_op_t : jet_test_t, WithParamInterface<jet_vector_op_vector_t>
{
    sut_t       lhs      = GetParam().lhs;
    sut_t const rhs      = GetParam().rhs;
    sut_t const expected = GetParam().expected;
};

// --------------------------------------------------------------------------------------------------------------------
// Addition
// --------------------------------------------------------------------------------------------------------------------

struct jet_test_addition_t : jet_test_vector_op_t
{};

TEST_P(jet_test_addition_t, compound_assign)
{
    auto const& actual = lhs += rhs;

    EXPECT_EQ(&lhs, &actual);
    EXPECT_EQ(expected, actual);
}

TEST_P(jet_test_addition_t, binary_op)
{
    auto const actual = lhs + rhs;

    EXPECT_EQ(expected, actual);
}

TEST_P(jet_test_addition_t, binary_op_commuted)
{
    auto const actual = rhs + lhs;

    EXPECT_EQ(expected, actual);
}

jet_vector_op_vector_t const addition_vectors[] = {
    {"positve + positive", {1.0, 2.0}, {3.0, 4.0}, {4.0, 6.0}},
    {"mixed signs", {-1.0, 2.0}, {3.0, -4.0}, {2.0, -2.0}},
    {"zeros", {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}},
    {"mixed zeros", {5.0, 0.0}, {0.0, 3.0}, {5.0, 3.0}},
};
INSTANTIATE_TEST_SUITE_P(addition, jet_test_addition_t, ValuesIn(addition_vectors),
                         test_name_generator_t<jet_vector_op_vector_t>{});

// --------------------------------------------------------------------------------------------------------------------
// Subtraction
// --------------------------------------------------------------------------------------------------------------------

struct jet_test_subtraction_t : jet_test_vector_op_t
{};

TEST_P(jet_test_subtraction_t, compound_assign)
{
    auto const& actual = lhs -= rhs;

    EXPECT_EQ(&lhs, &actual);
    EXPECT_EQ(expected, actual);
}

TEST_P(jet_test_subtraction_t, binary_op)
{
    auto const actual = lhs - rhs;

    EXPECT_EQ(expected, actual);
}

TEST_P(jet_test_subtraction_t, binary_op_commuted)
{
    auto const actual = rhs - lhs;

    EXPECT_EQ(-expected, actual);
}

jet_vector_op_vector_t const subtraction_vectors[] = {
    {"positve - smaller", {11.0, 17.0}, {5.0, 7.0}, {6.0, 10.0}},
    {"positve - larger", {5.0, 7.0}, {11.0, 17.0}, {-6.0, -10.0}},
    {"mixed signs", {-5.0, 7.0}, {11.0, -17.0}, {-16.0, 24.0}},
    {"zeros", {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}},
    {"mixed zeros", {5.0, 0.0}, {0.0, 7.0}, {5.0, -7.0}},
};
INSTANTIATE_TEST_SUITE_P(subtraction, jet_test_subtraction_t, ValuesIn(subtraction_vectors),
                         test_name_generator_t<jet_vector_op_vector_t>{});

} // namespace
} // namespace crv
