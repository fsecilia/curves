// SPDX-License-Identifier: MIT
/**
    \file
    \copyright Copyright (C) 2026 Frank Secilia
*/

#include <crv/math/jet/jet.hpp>
#include <crv/test/test.hpp>
#include <limits>
#include <numbers>
#include <ostream>
#include <sstream>

namespace crv {
namespace {

using value_t = real_t;
using sut_t   = jet_t<value_t>;

static constexpr auto eps = epsilon<value_t>();
static constexpr auto inf = infinity<value_t>();
static constexpr auto nan = std::numeric_limits<value_t>::quiet_NaN();
static constexpr auto e   = std::numbers::e;
static constexpr auto pi  = std::numbers::pi;

struct jet_test_t : Test
{
    static constexpr value_t f  = 37.2; // arbitrary
    static constexpr value_t df = 26.3; // arbitrary
    static constexpr sut_t   x{f, df};
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
    static_assert(value_t{} == derivative(f));
    static_assert(value_t{} == derivative(-f));
    static_assert(value_t{} == derivative(df));
}

// ====================================================================================================================
// Construction
// ====================================================================================================================

struct jet_test_construction_t : jet_test_t
{};

TEST_F(jet_test_construction_t, default_construction)
{
    constexpr auto sut = sut_t{};

    static_assert(0.0 == sut.f);
    static_assert(0.0 == sut.df);
}

TEST_F(jet_test_construction_t, value)
{
    constexpr auto sut = jet_t{f};

    static_assert(f == sut.f);
    static_assert(0.0 == sut.df);
}

TEST_F(jet_test_construction_t, pair)
{
    static_assert(f == x.f);
    static_assert(df == x.df);
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

    static_assert(static_cast<value_t>(f_int) == sut.f);
    static_assert(static_cast<value_t>(df_int) == sut.df);
}

TEST_F(jet_test_conversion_t, assign)
{
    constexpr auto jet_int = jet_t{f_int, df_int};
    auto           sut     = sut_t{};

    sut = jet_int;

    EXPECT_DOUBLE_EQ(static_cast<value_t>(f_int), sut.f);
    EXPECT_DOUBLE_EQ(static_cast<value_t>(df_int), sut.df);
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
// Value Arithmetic
// ====================================================================================================================

struct value_op_test_vector_t
{
    std::string name;
    sut_t       jet;
    value_t     value;
    sut_t       expected;

    friend auto operator<<(std::ostream& out, value_op_test_vector_t const& src) -> std::ostream&
    {
        return out << src.jet << " ⨂ " << src.value << " = " << src.expected;
    }
};

struct jet_test_value_op_t : jet_test_t, WithParamInterface<value_op_test_vector_t>
{
    sut_t         jet      = GetParam().jet;
    value_t const value    = GetParam().value;
    sut_t const&  expected = GetParam().expected;
};

// --------------------------------------------------------------------------------------------------------------------
// Value Addition
// --------------------------------------------------------------------------------------------------------------------

struct jet_test_value_addition_t : jet_test_value_op_t
{};

TEST_P(jet_test_value_addition_t, compound_assign)
{
    auto const& actual = jet += value;

    EXPECT_EQ(&jet, &actual);
    EXPECT_EQ(expected, actual);
}

TEST_P(jet_test_value_addition_t, jet_plus_value)
{
    auto const actual = jet + value;

    EXPECT_EQ(expected, actual);
}

TEST_P(jet_test_value_addition_t, value_plus_jet)
{
    auto const actual = value + jet;

    EXPECT_EQ(expected, actual);
}

// clang-format off
value_op_test_vector_t const value_addition_vectors[] = {
    {"positve + positive", {3.0, 5.0}, 11, {14.0, 5.0}},
    {"negative + negative", {-3.0, -5.0}, -11, {-14.0, -5.0}},
    {"mixed signs", {3.0, -5.0}, -11, {-8.0, -5.0}},
    {"zeros", {0.0, 0.0}, 0.0, {0.0, 0.0}},
    {"mixed zeros", {5.0, 0.0}, 0.0, {5.0, 0.0}},
};
// clang-format on
INSTANTIATE_TEST_SUITE_P(value_addition, jet_test_value_addition_t, ValuesIn(value_addition_vectors),
                         test_name_generator_t<value_op_test_vector_t>{});

// --------------------------------------------------------------------------------------------------------------------
// Value Subtraction
// --------------------------------------------------------------------------------------------------------------------

struct jet_test_value_subtraction_t : jet_test_value_op_t
{};

TEST_P(jet_test_value_subtraction_t, compound_assign)
{
    auto const& actual = jet -= value;

    EXPECT_EQ(&jet, &actual);
    EXPECT_EQ(expected, actual);
}

TEST_P(jet_test_value_subtraction_t, jet_minus_value)
{
    auto const actual = jet - value;

    EXPECT_EQ(expected, actual);
}

TEST_P(jet_test_value_subtraction_t, value_minus_jet)
{
    auto const actual = value - jet;

    EXPECT_EQ(-expected, actual);
}

// clang-format off
value_op_test_vector_t const value_subtraction_vectors[] = {
    {"positve - positive", {3.0, 5.0}, 11, {-8.0, 5.0}},
    {"negative - negative", {-3.0, -5.0}, -11, {8.0, -5.0}},
    {"mixed signs", {3.0, -5.0}, -11, {14.0, -5.0}},
    {"zeros", {0.0, 0.0}, 0.0, {0.0, 0.0}},
    {"mixed zeros", {5.0, 0.0}, 0.0, {5.0, 0.0}},
};
// clang-format on
INSTANTIATE_TEST_SUITE_P(value_subtraction, jet_test_value_subtraction_t, ValuesIn(value_subtraction_vectors),
                         test_name_generator_t<value_op_test_vector_t>{});

// --------------------------------------------------------------------------------------------------------------------
// Value Multiplication
// --------------------------------------------------------------------------------------------------------------------

struct jet_test_value_multiplication_t : jet_test_value_op_t
{};

TEST_P(jet_test_value_multiplication_t, compound_assign)
{
    auto const& actual = jet *= value;

    EXPECT_EQ(&jet, &actual);
    EXPECT_EQ(expected, actual);
}

TEST_P(jet_test_value_multiplication_t, jet_times_value)
{
    auto const actual = jet * value;

    EXPECT_EQ(expected, actual);
}

TEST_P(jet_test_value_multiplication_t, value_times_jet)
{
    auto const actual = value * jet;

    EXPECT_EQ(expected, actual);
}

// clang-format off
value_op_test_vector_t const value_multiplication_vectors[] = {
    {"positive*positive", {3.0, 2.0}, 5.0, {15.0, 10.0}},
    {"positive*negative", {3.0, 2.0}, -5.0, {-15.0, -10.0}},
    {"negative*positive", {-3.0, -2.0}, 5.0, {-15.0, -10.0}},
    {"negative*negative", {-3.0, -2.0}, -5.0, {15.0, 10.0}},
    {"*0", {3.0, 2.0}, 0.0, {0.0, 0.0}},
    {"0*", {0.0, 0.0}, 5.0, {0.0, 0.0}},
    {"0*0", {0.0, 0.0}, 0.0, {0.0, 0.0}},
};
// clang-format on
INSTANTIATE_TEST_SUITE_P(value_multiplication, jet_test_value_multiplication_t, ValuesIn(value_multiplication_vectors),
                         test_name_generator_t<value_op_test_vector_t>{});

// --------------------------------------------------------------------------------------------------------------------
// Value Division
// --------------------------------------------------------------------------------------------------------------------

struct jet_test_value_division_t : jet_test_value_op_t
{};

TEST_P(jet_test_value_division_t, compound_assign)
{
    auto const& actual = jet /= value;

    EXPECT_EQ(&jet, &actual);
    EXPECT_EQ(expected, actual);
}

TEST_P(jet_test_value_division_t, jet_divided_by_value)
{
    auto const actual = jet / value;

    EXPECT_EQ(expected, actual);
}

TEST_P(jet_test_value_division_t, value_divided_by_jet)
{
    auto const actual   = value / jet;
    auto const identity = actual * expected;

    EXPECT_NEAR(1.0, identity.f, 1e-15);
    EXPECT_NEAR(0.0, identity.df, 1e-15);
}

// clang-format off
value_op_test_vector_t const value_division_vectors[] = {
    {"positive/positive", {15.0, 10.0}, 5.0, {3.0, 2.0}},
    {"positive/negative", {15.0, 10.0}, -5.0, {-3.0, -2.0}},
    {"negative/positive", {-15.0, -10.0}, 5.0, {-3.0, -2.0}},
    {"negative/negative", {-15.0, -10.0}, -5.0, {3.0, 2.0}},
};
// clang-format on
INSTANTIATE_TEST_SUITE_P(value_division, jet_test_value_division_t, ValuesIn(value_division_vectors),
                         test_name_generator_t<value_op_test_vector_t>{});

// ====================================================================================================================
// Jet Arithmetic
// ====================================================================================================================

struct jet_op_test_vector_t
{
    std::string name;
    sut_t       lhs;
    sut_t       rhs;
    sut_t       expected;

    friend auto operator<<(std::ostream& out, jet_op_test_vector_t const& src) -> std::ostream&
    {
        return out << src.lhs << " ⨂ " << src.rhs << " = " << src.expected;
    }
};

struct jet_test_jet_op_t : jet_test_t, WithParamInterface<jet_op_test_vector_t>
{
    sut_t       lhs      = GetParam().lhs;
    sut_t const rhs      = GetParam().rhs;
    sut_t const expected = GetParam().expected;
};

// --------------------------------------------------------------------------------------------------------------------
// Addition
// --------------------------------------------------------------------------------------------------------------------

struct jet_test_jet_addition_t : jet_test_jet_op_t
{};

TEST_P(jet_test_jet_addition_t, compound_assign)
{
    auto const& actual = lhs += rhs;

    EXPECT_EQ(&lhs, &actual);
    EXPECT_EQ(expected, actual);
}

TEST_P(jet_test_jet_addition_t, binary_op)
{
    auto const actual = lhs + rhs;

    EXPECT_EQ(expected, actual);
}

TEST_P(jet_test_jet_addition_t, binary_op_commuted)
{
    auto const actual = rhs + lhs;

    EXPECT_EQ(expected, actual);
}

jet_op_test_vector_t const jet_addition_vectors[] = {
    {"positve + positive", {1.0, 2.0}, {3.0, 4.0}, {4.0, 6.0}},
    {"mixed signs", {-1.0, 2.0}, {3.0, -4.0}, {2.0, -2.0}},
    {"zeros", {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}},
    {"mixed zeros", {5.0, 0.0}, {0.0, 3.0}, {5.0, 3.0}},
};
INSTANTIATE_TEST_SUITE_P(jet_addition, jet_test_jet_addition_t, ValuesIn(jet_addition_vectors),
                         test_name_generator_t<jet_op_test_vector_t>{});

// --------------------------------------------------------------------------------------------------------------------
// Subtraction
// --------------------------------------------------------------------------------------------------------------------

struct jet_test_jet_subtraction_t : jet_test_jet_op_t
{};

TEST_P(jet_test_jet_subtraction_t, compound_assign)
{
    auto const& actual = lhs -= rhs;

    EXPECT_EQ(&lhs, &actual);
    EXPECT_EQ(expected, actual);
}

TEST_P(jet_test_jet_subtraction_t, binary_op)
{
    auto const actual = lhs - rhs;

    EXPECT_EQ(expected, actual);
}

TEST_P(jet_test_jet_subtraction_t, binary_op_commuted)
{
    auto const actual = rhs - lhs;

    EXPECT_EQ(-expected, actual);
}

jet_op_test_vector_t const jet_subtraction_vectors[] = {
    {"positve - smaller", {11.0, 17.0}, {5.0, 7.0}, {6.0, 10.0}},
    {"positve - larger", {5.0, 7.0}, {11.0, 17.0}, {-6.0, -10.0}},
    {"mixed signs", {-5.0, 7.0}, {11.0, -17.0}, {-16.0, 24.0}},
    {"zeros", {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}},
    {"mixed zeros", {5.0, 0.0}, {0.0, 7.0}, {5.0, -7.0}},
};
INSTANTIATE_TEST_SUITE_P(jet_subtraction, jet_test_jet_subtraction_t, ValuesIn(jet_subtraction_vectors),
                         test_name_generator_t<jet_op_test_vector_t>{});

// --------------------------------------------------------------------------------------------------------------------
// Multiplication
// --------------------------------------------------------------------------------------------------------------------

struct jet_test_jet_multiplication_t : jet_test_jet_op_t
{};

TEST_P(jet_test_jet_multiplication_t, compound_assign)
{
    auto const& actual = lhs *= rhs;

    EXPECT_EQ(&lhs, &actual);
    EXPECT_EQ(expected, actual);
}

TEST_P(jet_test_jet_multiplication_t, binary_op)
{
    auto const actual = lhs * rhs;

    EXPECT_EQ(expected, actual);
}

TEST_P(jet_test_jet_multiplication_t, binary_op_commuted)
{
    auto const actual = rhs * lhs;

    EXPECT_EQ(expected, actual);
}

/*
    product rule: d(u*v) = u*dv + du*v
    {f1, df1}*{f2, df2} = {f1*f2, f1*df2 + df1*f2}
*/
jet_op_test_vector_t const jet_multiplication_vectors[] = {
    // {2, 1}*{3, 1} = {2*3, 2*1 + 1*3} = {6, 5}
    {"variable*variable", {2.0, 1.0}, {3.0, 1.0}, {6.0, 5.0}},

    // {3, 2}*{5, 7} = {3*5, 3*7 + 2*5} = {15, 31}
    {"scaled_variable*scaled_variable", {3.0, 2.0}, {5.0, 7.0}, {15.0, 31.0}},

    // zero cases
    {"derivative*scaled_variable", {0.0, 2.0}, {5.0, 7.0}, {0.0, 10.0}},
    {"scaled_variable*zero", {3.0, 2.0}, {0.0, 0.0}, {0.0, 0.0}},

    // identity
    {"identity", {3.0, 2.0}, {1.0, 0.0}, {3.0, 2.0}},
};
INSTANTIATE_TEST_SUITE_P(jet_multiplication, jet_test_jet_multiplication_t, ValuesIn(jet_multiplication_vectors),
                         test_name_generator_t<jet_op_test_vector_t>{});

// --------------------------------------------------------------------------------------------------------------------
// Division
// --------------------------------------------------------------------------------------------------------------------

struct jet_test_jet_division_t : jet_test_jet_op_t
{};

TEST_P(jet_test_jet_division_t, compound_assign)
{
    auto const& actual = lhs /= rhs;

    EXPECT_EQ(&lhs, &actual);
    EXPECT_EQ(expected, actual);
}

TEST_P(jet_test_jet_division_t, binary_op)
{
    auto const actual = lhs / rhs;

    EXPECT_EQ(expected, actual);
}

TEST_P(jet_test_jet_division_t, binary_op_inverse)
{
    auto const actual   = rhs / lhs;
    auto const identity = actual * expected;

    EXPECT_NEAR(1.0, identity.f, 1e-15);
    EXPECT_NEAR(0.0, identity.df, 1e-15);
}

/*
    quotient rule: d(u/v) = (du*v - u*dv)/v^2 = (du - (u/v)*dv)/v
    {f1, df1} / {f2, df2} = {f1/f2, (df1 - (f1/f2)*df2)/f2}
*/
jet_op_test_vector_t const jet_division_vectors[] = {
    // {3, 2} / {1, 0} = {3/1, (1 - (3/2)*0)/1}
    {"scaled_variable/identity", {3.0, 2.0}, {1.0, 0.0}, {3.0, 2.0}},

    // {10, 4} / {2, 0} = {10/2, (4 - (10/2)*0)/2}
    {"scaled_variable/constant", {10.0, 4.0}, {2.0, 0.0}, {5.0, 2.0}},

    // {6, 5} / {2, 1} = {6/2, (5 - (6/2)*1)/2}
    {"scaled_variable/variable", {6.0, 5.0}, {2.0, 1.0}, {3.0, 1.0}},
};
INSTANTIATE_TEST_SUITE_P(jet_division, jet_test_jet_division_t, ValuesIn(jet_division_vectors),
                         test_name_generator_t<jet_op_test_vector_t>{});

// ====================================================================================================================
// Selection
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
// Min, Max
// --------------------------------------------------------------------------------------------------------------------

struct selection_min_max_test_vector_t
{
    std::string name;
    sut_t       x;
    sut_t       y;
    sut_t       expected_min;
    sut_t       expected_max;

    friend auto operator<<(std::ostream& out, selection_min_max_test_vector_t const& src) -> std::ostream&
    {
        return out << "{.name = \"" << src.name << "\", .x = " << src.x << ", .y = " << src.y
                   << ", .expected_min = " << src.expected_min << ", .expected_max = " << src.expected_max << "}";
    }
};

struct jet_test_selection_min_max_t : jet_test_t, WithParamInterface<selection_min_max_test_vector_t>
{
    sut_t const x            = GetParam().x;
    sut_t const y            = GetParam().y;
    sut_t const expected_min = GetParam().expected_min;
    sut_t const expected_max = GetParam().expected_max;
};

TEST_P(jet_test_selection_min_max_t, min)
{
    auto const actual = min(x, y);

    EXPECT_DOUBLE_EQ(expected_min.f, actual.f);
    EXPECT_DOUBLE_EQ(expected_min.df, actual.df);
}

TEST_P(jet_test_selection_min_max_t, max)
{
    auto const actual = max(x, y);

    EXPECT_DOUBLE_EQ(expected_max.f, actual.f);
    EXPECT_DOUBLE_EQ(expected_max.df, actual.df);
}

selection_min_max_test_vector_t const selection_vectors[] = {
    // x < y: min=x, max=y
    {"x < y = x, y", {2.0, 10.0}, {5.0, 20.0}, {2.0, 10.0}, {5.0, 20.0}},

    // y > x: min=y, max=x
    {"y > x = y, x", {5.0, 10.0}, {2.0, 20.0}, {2.0, 20.0}, {5.0, 10.0}},

    // x == y: min=x, max=x
    {"x == y = x, y", {3.0, 10.0}, {3.0, 20.0}, {3.0, 20.0}, {3.0, 20.0}},
};
INSTANTIATE_TEST_SUITE_P(selection, jet_test_selection_min_max_t, ValuesIn(selection_vectors),
                         test_name_generator_t<selection_min_max_test_vector_t>{});

// --------------------------------------------------------------------------------------------------------------------
// Clamp
// --------------------------------------------------------------------------------------------------------------------

struct jet_test_selection_clamp_t : jet_test_t
{};

TEST_F(jet_test_selection_clamp_t, below)
{
    auto const actual = clamp(sut_t{1.0, 3.0}, sut_t{2.0, 10.0}, sut_t{8.0, 20.0});

    EXPECT_DOUBLE_EQ(2.0, actual.f);
    EXPECT_DOUBLE_EQ(10.0, actual.df);
}

TEST_F(jet_test_selection_clamp_t, above)
{
    auto const actual = clamp(sut_t{10.0, 3.0}, sut_t{2.0, 10.0}, sut_t{8.0, 20.0});

    EXPECT_DOUBLE_EQ(8.0, actual.f);
    EXPECT_DOUBLE_EQ(20.0, actual.df);
}

TEST_F(jet_test_selection_clamp_t, within)
{
    auto const actual = clamp(sut_t{5.0, 3.0}, sut_t{2.0, 10.0}, sut_t{8.0, 20.0});

    EXPECT_DOUBLE_EQ(5.0, actual.f);
    EXPECT_DOUBLE_EQ(3.0, actual.df);
}

// ====================================================================================================================
// Classification
// ====================================================================================================================

struct classification_test_vector_t
{
    std::string name;
    sut_t       sut;
    bool        expected_isfinite;
    bool        expected_isinf;
    bool        expected_isnan;

    friend auto operator<<(std::ostream& out, classification_test_vector_t const& src) -> std::ostream&
    {
        return out << "{.name = \"" << src.name << "\", .sut = " << src.sut
                   << ", .expected_isfinite = " << src.expected_isfinite << ", .expected_isinf = " << src.expected_isinf
                   << ", .expected_isnan = " << src.expected_isnan << "}";
    }
};

struct jet_test_classification_t : jet_test_t, WithParamInterface<classification_test_vector_t>
{
    sut_t const& sut = GetParam().sut;

    bool const expected_isfinite = GetParam().expected_isfinite;
    bool const expected_isinf    = GetParam().expected_isinf;
    bool const expected_isnan    = GetParam().expected_isnan;
};

TEST_P(jet_test_classification_t, isfinite)
{
    EXPECT_EQ(expected_isfinite, isfinite(sut));
}

TEST_P(jet_test_classification_t, isinf)
{
    EXPECT_EQ(expected_isinf, isinf(sut));
}

TEST_P(jet_test_classification_t, isnan)
{
    EXPECT_EQ(expected_isnan, isnan(sut));
}

classification_test_vector_t const classification_vectors[] = {
    {"zero", {0.0, 0.0}, true, false, false},
    {"vector", {1.0, 2.0}, true, false, false},
    {"infinite primal", {inf, 0.0}, false, true, false},
    {"infinite derivative", {0.0, inf}, false, true, false},
    {"infinity", {inf, inf}, false, true, false},

    {"nan primal", {nan, 0.0}, false, false, true},
    {"nan derivative", {0.0, nan}, false, false, true},
    {"nan", {nan, nan}, false, false, true},

    {"infinite primal, nan derivative", {inf, nan}, false, false, true},
    {"nan primal, infinite derivative", {nan, inf}, false, false, true},
};
INSTANTIATE_TEST_SUITE_P(classification, jet_test_classification_t, ValuesIn(classification_vectors),
                         test_name_generator_t<classification_test_vector_t>{});

// ====================================================================================================================
// Math Functions
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
// One-Param Test
// --------------------------------------------------------------------------------------------------------------------

struct math_func_test_vector_x_t
{
    std::string name;
    sut_t       x;
    sut_t       expected;

    friend auto operator<<(std::ostream& out, math_func_test_vector_x_t const& src) -> std::ostream&
    {
        return out << "{.name = \"" << src.name << "\", .x = " << src.x << ", .expected = " << src.expected << "}";
    }
};

struct jet_test_math_func_x_t : jet_test_t, WithParamInterface<math_func_test_vector_x_t>
{
    sut_t const& x        = GetParam().x;
    sut_t const& expected = GetParam().expected;
};

// --------------------------------------------------------------------------------------------------------------------
// Two-Param Test
// --------------------------------------------------------------------------------------------------------------------

struct math_func_test_vector_xy_t
{
    std::string name;
    sut_t       x;
    sut_t       y;
    sut_t       expected;

    friend auto operator<<(std::ostream& out, math_func_test_vector_xy_t const& src) -> std::ostream&
    {
        return out << "{.name = \"" << src.name << "\", .x = " << src.x << ", .y = " << src.y
                   << ", .expected = " << src.expected << "}";
    }
};

struct jet_test_math_func_xy_t : jet_test_t, WithParamInterface<math_func_test_vector_xy_t>
{
    sut_t const& x        = GetParam().x;
    sut_t const& y        = GetParam().y;
    sut_t const& expected = GetParam().expected;
};

// --------------------------------------------------------------------------------------------------------------------
// abs
// --------------------------------------------------------------------------------------------------------------------

struct jet_test_abs_t : jet_test_math_func_x_t
{};

TEST_P(jet_test_abs_t, result)
{
    auto const actual = abs(x);

    EXPECT_DOUBLE_EQ(expected.f, actual.f);
    EXPECT_DOUBLE_EQ(expected.df, actual.df);
}

// clang-format off
// d(|x|) = sgn(x)*dx
math_func_test_vector_x_t const abs_vectors[] = {
    {"negative, negative", {-7.0, -13.0}, {7.0,  13.0}},
    {"negative, zero",     {-7.0,   0.0}, {7.0,   0.0}},
    {"negative, positive", {-7.0,  13.0}, {7.0, -13.0}},
    {"zero,     negative", { 0.0, -13.0}, {0.0, -13.0}},
    {"zero,     zero",     { 0.0,   0.0}, {0.0,   0.0}},
    {"zero,     positive", { 0.0,  13.0}, {0.0,  13.0}},
    {"positive, negative", { 7.0, -13.0}, {7.0, -13.0}},
    {"positive, zero",     { 7.0,   0.0}, {7.0,   0.0}},
    {"positive, positive", { 7.0,  13.0}, {7.0,  13.0}},
};
// clang-format on
INSTANTIATE_TEST_SUITE_P(abs, jet_test_abs_t, ValuesIn(abs_vectors),
                         test_name_generator_t<math_func_test_vector_x_t>{});

// --------------------------------------------------------------------------------------------------------------------
// copysign
// --------------------------------------------------------------------------------------------------------------------

struct jet_test_copysign_t : jet_test_math_func_xy_t
{};

TEST_P(jet_test_copysign_t, result)
{
    auto const actual = copysign(x, y);

    EXPECT_DOUBLE_EQ(expected.f, actual.f);
    EXPECT_DOUBLE_EQ(expected.df, actual.df);
}

// clang-format off
// d(copysign(x, y)) = sgn(x)*dx*sgn(y) + |x|*delta(y)*dy
math_func_test_vector_xy_t const copysign_vectors[] = {
    // standard cases
    {"nn, nn", {-7.0, -11.0}, {-5.0, -19.0}, {-7.0, -11.0}},
    {"pp, nn", { 7.0,  11.0}, {-5.0, -19.0}, {-7.0, -11.0}},
    {"pp, pp", { 7.0,  11.0}, { 5.0,  19.0}, { 7.0,  11.0}},

    // singularities
    {"pp, 0p", { 7.0,  11.0}, { 0.0,  19.0}, { 7.0,   inf}},
    {"pp, 0n", { 7.0,  11.0}, { 0.0, -19.0}, { 7.0,  -inf}},

    // successful guards of singularity
    {"0n, 0n", { 0.0, -11.0}, { 0.0, -19.0}, { 0.0, -11.0}},
    {"00, 00", { 0.0,   0.0}, { 0.0,   0.0}, { 0.0,   0.0}},
    {"pp, 00", { 7.0,  11.0}, { 0.0,   0.0}, { 7.0,  11.0}},
};
// clang-format on
INSTANTIATE_TEST_SUITE_P(copysign, jet_test_copysign_t, ValuesIn(copysign_vectors),
                         test_name_generator_t<math_func_test_vector_xy_t>{});

// --------------------------------------------------------------------------------------------------------------------
// copysign Edge Cases
// --------------------------------------------------------------------------------------------------------------------

struct jet_test_copysign_edge_cases_t : jet_test_t
{};

TEST_F(jet_test_copysign_edge_cases_t, negative_zero_still_flips_sign)
{
    auto const actual = copysign(jet_t{3.0, 1.0}, jet_t{-0.0, 1.0});

    EXPECT_DOUBLE_EQ(-3.0, actual.f);
}

TEST_F(jet_test_copysign_edge_cases_t, negative_zero_sign_bit_perserved)
{
    auto const actual = copysign(jet_t{0.0, 1.0}, jet_t{-0.0, 1.0});

    EXPECT_DOUBLE_EQ(0.0, actual.f);
    EXPECT_TRUE(std::signbit(actual.f));
}

// --------------------------------------------------------------------------------------------------------------------
// cos
// --------------------------------------------------------------------------------------------------------------------

struct jet_test_cos_t : jet_test_math_func_x_t
{};

TEST_P(jet_test_cos_t, result)
{
    auto const actual = cos(x);

    EXPECT_NEAR(expected.f, actual.f, eps);
    EXPECT_NEAR(expected.df, actual.df, eps);
}

// clang-format off
// d(cos(x)) = -sin(x)*dx
const math_func_test_vector_x_t cos_vectors[] = {
    {"0", {0.0, 1.3}, {1.0, 0.0}},
    {"-pi/2", {-pi/2, 1.3}, {0.0, 1.3}},
    {"pi/5", {pi / 5, 1.3}, {cos(pi / 5), -1.3*sin(pi / 5)}},
    {"pi/3", {pi / 3, 1.3}, {cos(pi / 3), -1.3*sin(pi / 3)}},
    {"pi/2,", {pi/2, 1.3}, {0.0, -1.3}},
    {"pi", {pi, 1.3}, {-1.0, 0.0}},
};
// clang-format on
INSTANTIATE_TEST_SUITE_P(cos, jet_test_cos_t, ValuesIn(cos_vectors),
                         test_name_generator_t<math_func_test_vector_x_t>{});

// --------------------------------------------------------------------------------------------------------------------
// cosh
// --------------------------------------------------------------------------------------------------------------------

struct jet_test_cosh_t : jet_test_math_func_x_t
{};

TEST_P(jet_test_cosh_t, result)
{
    auto const actual = cosh(x);

    EXPECT_NEAR(expected.f, actual.f, eps);
    EXPECT_NEAR(expected.df, actual.df, eps);
}

// d(cosh(x)) = sinh(x)*dx
constexpr auto cosh_vector(std::string name, value_t angle) noexcept -> math_func_test_vector_x_t
{
    auto const dx = value_t{1.3};
    return {std::move(name), {angle, dx}, {cosh(angle), sinh(angle) * dx}};
}

// clang-format off
math_func_test_vector_x_t const cosh_vectors[] = {
    {cosh_vector("-1", -1.0)},
    {cosh_vector("0", 0.0)},
    {cosh_vector("1/5", 1.0 / 5)},
    {cosh_vector("1/4", 1.0 / 4)},
    {cosh_vector("1/3", 1.0 / 3)},
    {cosh_vector("1/2", 1.0 / 2)},
    {cosh_vector("1", 1.0)},
};
// clang-format on
INSTANTIATE_TEST_SUITE_P(cosh, jet_test_cosh_t, ValuesIn(cosh_vectors),
                         test_name_generator_t<math_func_test_vector_x_t>{});

// --------------------------------------------------------------------------------------------------------------------
// exp
// --------------------------------------------------------------------------------------------------------------------

struct jet_test_exp_t : jet_test_math_func_x_t
{};

TEST_P(jet_test_exp_t, result)
{
    auto const actual = exp(x);

    EXPECT_NEAR(expected.f, actual.f, 4 * eps);
    EXPECT_NEAR(expected.df, actual.df, eps);
}

// clang-format off
// d(exp(x)) = exp(x)*dx
const math_func_test_vector_x_t exp_vectors[] = {
    {"-1", {-1.0, 1.3}, {1.0 / e, 1.3 / e}},
    {"0", {0.0, 1.3}, {1.0, 1.3}},
    {"0.5", {0.5, 1.3}, {sqrt(e), 1.3*sqrt(e)}},
    {"1", {1.0, 1.3}, {e, 1.3*e}},
    {"2", {2.0, 1.3}, {e*e, e*e*1.3}},
};
// clang-format on
INSTANTIATE_TEST_SUITE_P(exp, jet_test_exp_t, ValuesIn(exp_vectors),
                         test_name_generator_t<math_func_test_vector_x_t>{});

// --------------------------------------------------------------------------------------------------------------------
// hypot
// --------------------------------------------------------------------------------------------------------------------

struct jet_test_hypot_t : jet_test_math_func_xy_t
{};

TEST_P(jet_test_hypot_t, result)
{
    auto const actual = hypot(x, y);

    EXPECT_DOUBLE_EQ(expected.f, actual.f);
    EXPECT_DOUBLE_EQ(expected.df, actual.df);
}

// clang-format off
// d(hypot(x, y)) = (x*dx + y*dy) / hypot(x, y)
math_func_test_vector_xy_t const hypot_vectors[] = {
    {"{0, 0.0}, {0, 0.0}}", {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.00}},
    {"{0, 1.3}, {0, 1.3}}", {0.0, 1.3}, {0.0, 1.3}, {0.0, 0.00}},
    {"{3, 1.3}, {4, 0.0}}", {3.0, 1.3}, {4.0, 0.0}, {5.0, 0.78}},
    {"{3, 0.0}, {4, 1.3}}", {3.0, 0.0}, {4.0, 1.3}, {5.0, 1.04}},
    {"{3, 1.3}, {4, 1.3}}", {3.0, 1.3}, {4.0, 1.3}, {5.0, 1.82}},
};
// clang-format on
INSTANTIATE_TEST_SUITE_P(hypot, jet_test_hypot_t, ValuesIn(hypot_vectors),
                         test_name_generator_t<math_func_test_vector_xy_t>{});

// --------------------------------------------------------------------------------------------------------------------
// log
// --------------------------------------------------------------------------------------------------------------------

struct jet_test_log_t : jet_test_math_func_x_t
{};

TEST_P(jet_test_log_t, result)
{
    auto const actual = log(x);

    EXPECT_NEAR(expected.f, actual.f, eps);
    EXPECT_NEAR(expected.df, actual.df, eps);
}

// clang-format off
// d(log(x)) = dx/x
const math_func_test_vector_x_t log_vectors[] = {
    {"0.5", {0.5, 1.3}, {log(0.5), 1.3/0.5}},
    {"1", {1.0, 1.3}, {0.0, 1.3}},
    {"e", {e, 1.3}, {1.0, 1.3/e}},
    {"2", {2.0, 1.3}, {log(2), 1.3/2}},
    {"10", {10.0, 1.3}, {log(10), 1.3/10}},
};
// clang-format on
INSTANTIATE_TEST_SUITE_P(log, jet_test_log_t, ValuesIn(log_vectors),
                         test_name_generator_t<math_func_test_vector_x_t>{});

// --------------------------------------------------------------------------------------------------------------------
// log1p
// --------------------------------------------------------------------------------------------------------------------

struct jet_test_log1p_t : jet_test_math_func_x_t
{};

TEST_P(jet_test_log1p_t, result)
{
    auto const actual = log1p(x);

    EXPECT_NEAR(expected.f, actual.f, eps);
    EXPECT_NEAR(expected.df, actual.df, eps);
}

// clang-format off
// d(log1p(x)) = dx/x
const math_func_test_vector_x_t log1p_vectors[] = {
    {"0", {0.0, 1.3}, {log1p(0.0), 1.3}},
    {"0.5", {0.5, 1.3}, {log1p(0.5), 1.3/1.5}},
    {"1", {1.0, 1.3}, {log1p(1.0), 1.3/2}},
    {"e", {e, 1.3}, {log1p(e), 1.3/(e + 1)}},
    {"2", {2.0, 1.3}, {log1p(2), 1.3/3}},
    {"10", {10.0, 1.3}, {log1p(10), 1.3/11}},
};
// clang-format on
INSTANTIATE_TEST_SUITE_P(log1p, jet_test_log1p_t, ValuesIn(log1p_vectors),
                         test_name_generator_t<math_func_test_vector_x_t>{});

// --------------------------------------------------------------------------------------------------------------------
// pow
// --------------------------------------------------------------------------------------------------------------------

struct jet_test_pow_t : jet_test_math_func_xy_t
{};

TEST_P(jet_test_pow_t, jet_to_jet)
{
    auto const actual = pow(x, y);

    EXPECT_NEAR(expected.f, actual.f, eps);
    EXPECT_NEAR(expected.df, actual.df, eps);
}

TEST_P(jet_test_pow_t, jet_to_element)
{
    auto const reference = pow(x, jet_t{y.f, 0.0});
    auto const actual    = pow(x, y.f);

    EXPECT_NEAR(reference.f, actual.f, eps);
    EXPECT_NEAR(reference.df, actual.df, eps);
}

TEST_P(jet_test_pow_t, element_to_jet)
{
    auto const reference = pow(jet_t{x.f, 0.0}, y);
    auto const actual    = pow(x.f, y);

    EXPECT_NEAR(reference.f, actual.f, eps);
    EXPECT_NEAR(reference.df, actual.df, eps);
}

// d(x^y) = x^y*log(x)*dy + x^(y - 1)*y*dx
auto pow_reference(sut_t const& lhs, sut_t const& rhs) noexcept -> sut_t
{
    auto const x  = lhs.f;
    auto const y  = rhs.f;
    auto const dx = lhs.df;
    auto const dy = rhs.df;
    return {pow(x, y), pow(x, y) * log(x) * dy + pow(x, y - 1.0) * y * dx};
}

auto pow_vector(std::string name, sut_t const& lhs, sut_t const& rhs) noexcept -> math_func_test_vector_xy_t
{
    return {std::move(name), lhs, rhs, pow_reference(lhs, rhs)};
}

// clang-format off
const math_func_test_vector_xy_t pow_vectors[] = {
    pow_vector("-e", {5.1, 1.3}, {-e, 2.4}),
    pow_vector("-1", {5.1, 1.3}, {-1.0, 2.4}),
    pow_vector("0", {5.1, 1.3}, {0.0, 2.4}),
    pow_vector("sqrt", {5.1, 1.3}, {0.5, 2.4}),
    pow_vector("1", {5.1, 1.3}, {1.0, 2.4}),
    pow_vector("e^e", {e, 1.3}, {e, 2.4}),
    pow_vector("e", {5.1, 1.3}, {e, 2.4}),
    pow_vector("square", {5.1, 1.3}, {2.0, 2.4}),
    pow_vector("cube", {5.1, 1.3}, {3.0, 2.4}),
};
// clang-format on
INSTANTIATE_TEST_SUITE_P(pow, jet_test_pow_t, ValuesIn(pow_vectors),
                         test_name_generator_t<math_func_test_vector_xy_t>{});

// --------------------------------------------------------------------------------------------------------------------
// sin
// --------------------------------------------------------------------------------------------------------------------

struct jet_test_sin_t : jet_test_math_func_x_t
{};

TEST_P(jet_test_sin_t, result)
{
    auto const actual = sin(x);

    EXPECT_NEAR(expected.f, actual.f, eps);
    EXPECT_NEAR(expected.df, actual.df, eps);
}

// clang-format off
// d(sin(x)) = -cos(x)*dx
const math_func_test_vector_x_t sin_vectors[] = {
    {"0", {0.0, 1.3}, {0.0, 1.3}},
    {"-pi/2", {-pi/2, 1.3}, {-1.0, 0}},
    {"pi/5", {pi / 5, 1.3}, {sin(pi / 5), 1.3*cos(pi / 5)}},
    {"pi/3", {pi / 3, 1.3}, {sin(pi / 3), 1.3*cos(pi / 3)}},
    {"pi/2,", {pi/2, 1.3}, {1.0, 0}},
    {"pi", {pi, 1.3}, {0.0, -1.3}},
};
// clang-format on
INSTANTIATE_TEST_SUITE_P(sin, jet_test_sin_t, ValuesIn(sin_vectors),
                         test_name_generator_t<math_func_test_vector_x_t>{});

// --------------------------------------------------------------------------------------------------------------------
// sinh
// --------------------------------------------------------------------------------------------------------------------

struct jet_test_sinh_t : jet_test_math_func_x_t
{};

TEST_P(jet_test_sinh_t, result)
{
    auto const actual = sinh(x);

    EXPECT_NEAR(expected.f, actual.f, eps);
    EXPECT_NEAR(expected.df, actual.df, eps);
}

// d(sinh(x)) = cosh(x)*dx
constexpr auto sinh_vector(std::string name, value_t angle) noexcept -> math_func_test_vector_x_t
{
    auto const dx = value_t{1.3};
    return {std::move(name), {angle, dx}, {sinh(angle), cosh(angle) * dx}};
}

// clang-format off
math_func_test_vector_x_t const sinh_vectors[] = {
    {sinh_vector("-1", -1.0)},
    {sinh_vector("0", 0.0)},
    {sinh_vector("1/5", 1.0 / 5)},
    {sinh_vector("1/4", 1.0 / 4)},
    {sinh_vector("1/3", 1.0 / 3)},
    {sinh_vector("1/2", 1.0 / 2)},
    {sinh_vector("1", 1.0)},
};
// clang-format on
INSTANTIATE_TEST_SUITE_P(sinh, jet_test_sinh_t, ValuesIn(sinh_vectors),
                         test_name_generator_t<math_func_test_vector_x_t>{});

// --------------------------------------------------------------------------------------------------------------------
// sqrt
// --------------------------------------------------------------------------------------------------------------------

struct jet_test_sqrt_t : jet_test_math_func_x_t
{};

TEST_P(jet_test_sqrt_t, result)
{
    auto const actual = sqrt(x);

    EXPECT_NEAR(expected.f, actual.f, eps);
    EXPECT_NEAR(expected.df, actual.df, eps);
}

// clang-format off
// d(sqrt(x)) = dx/(2*sqrt(x))
const math_func_test_vector_x_t sqrt_vectors[] = {
    {"0", {0.0, 1.3}, {0.0, inf}},
    {"0.25", {0.25, 1.3}, {0.5, 1.3}},
    {"1.0", {1.0, 1.3}, {1.0, 1.3/2.0}},
    {"4.0", {4.0, 1.3}, {2.0, 1.3/4.0}},
    {"8.0", {8.0, 1.3}, {sqrt(8.0), 1.3/(2*sqrt(8.0))}},
    {"9.0", {9.0, 1.3}, {3.0, 1.3/6.0}},
    {"16.0", {16.0, 1.3}, {4.0, 1.3/8.0}},
};
// clang-format on
INSTANTIATE_TEST_SUITE_P(sqrt, jet_test_sqrt_t, ValuesIn(sqrt_vectors),
                         test_name_generator_t<math_func_test_vector_x_t>{});

// --------------------------------------------------------------------------------------------------------------------
// tan
// --------------------------------------------------------------------------------------------------------------------

struct jet_test_tan_t : jet_test_math_func_x_t
{};

TEST_P(jet_test_tan_t, result)
{
    auto const actual = tan(x);

    EXPECT_NEAR(expected.f, actual.f, eps);
    EXPECT_NEAR(expected.df, actual.df, eps);
}

// d(tan(x)) = (1 + tan(x)^2)*dx
constexpr auto tan_vector(std::string name, value_t a) noexcept -> math_func_test_vector_x_t
{
    auto const tan_a = tan(a);
    auto const dx    = 1.3;
    return {std::move(name), jet_t{a, dx}, jet_t{tan_a, (1.0 + tan_a * tan_a) * dx}};
}

// clang-format off
const math_func_test_vector_x_t tan_vectors[] = {
    tan_vector("-pi/4", -pi/4),
    tan_vector("0", 0.0),
    tan_vector("pi/5", pi/5),
    tan_vector("pi/4", pi/4),
    tan_vector("pi/3", pi/3),
    tan_vector("pi/2", pi/2),
    tan_vector("pi", pi),
};
// clang-format on
INSTANTIATE_TEST_SUITE_P(tan, jet_test_tan_t, ValuesIn(tan_vectors),
                         test_name_generator_t<math_func_test_vector_x_t>{});

// --------------------------------------------------------------------------------------------------------------------
// tanh
// --------------------------------------------------------------------------------------------------------------------

struct jet_test_tanh_t : jet_test_math_func_x_t
{};

TEST_P(jet_test_tanh_t, result)
{
    auto const actual = tanh(x);

    EXPECT_NEAR(expected.f, actual.f, eps);
    EXPECT_NEAR(expected.df, actual.df, eps);
}

// d(tanh(x)) = (1 - tanh(x)^2)*dx
constexpr auto tanh_vector(std::string name, value_t a) noexcept -> math_func_test_vector_x_t
{
    auto const tanh_a = tanh(a);
    auto const dx     = 1.3;
    return {std::move(name), jet_t{a, dx}, jet_t{tanh_a, (1.0 - tanh_a * tanh_a) * dx}};
}

// clang-format off
const math_func_test_vector_x_t tanh_vectors[] = {
    tanh_vector("-1", -1.0),
    tanh_vector("0", 0.0),
    tanh_vector("1", 1.0),
    tanh_vector("5", 5.0),
};
// clang-format on
INSTANTIATE_TEST_SUITE_P(tanh, jet_test_tanh_t, ValuesIn(tanh_vectors),
                         test_name_generator_t<math_func_test_vector_x_t>{});

// ====================================================================================================================
// Standard Library Integration
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
// ostream Inserter
// --------------------------------------------------------------------------------------------------------------------

struct jet_test_ostream_inserter_t : jet_test_t
{};

TEST_F(jet_test_ostream_inserter_t, format)
{
    std::ostringstream out;

    out << jet_t{3.5, 2.5};

    EXPECT_EQ(out.str(), "{.f = 3.5, .df = 2.5}");
}

// --------------------------------------------------------------------------------------------------------------------
// Swap
// --------------------------------------------------------------------------------------------------------------------

struct jet_test_swap_t : jet_test_t
{};

TEST_F(jet_test_swap_t, swap)
{
    auto lhs = sut_t{1.0, 2.0};
    auto rhs = sut_t{3.0, 4.0};

    swap(lhs, rhs);

    EXPECT_EQ(lhs.f, 3.0);
    EXPECT_EQ(lhs.df, 4.0);
    EXPECT_EQ(rhs.f, 1.0);
    EXPECT_EQ(rhs.df, 2.0);
}

// --------------------------------------------------------------------------------------------------------------------
// numeric_limits
// --------------------------------------------------------------------------------------------------------------------

struct jet_test_numeric_limits_t : jet_test_t
{};

TEST_F(jet_test_numeric_limits_t, is_specialized)
{
    static_assert(std::numeric_limits<sut_t>::is_specialized);
}

TEST_F(jet_test_numeric_limits_t, min)
{
    constexpr auto min = std::numeric_limits<sut_t>::min();

    static_assert(primal(min) == std::numeric_limits<value_t>::min());
}

TEST_F(jet_test_numeric_limits_t, max)
{
    constexpr auto max = std::numeric_limits<sut_t>::max();

    static_assert(primal(max) == std::numeric_limits<value_t>::max());
}

TEST_F(jet_test_numeric_limits_t, infinity)
{
    constexpr auto inf = std::numeric_limits<sut_t>::infinity();

    static_assert(std::isinf(primal(inf)));
}

TEST_F(jet_test_numeric_limits_t, quiet_nan)
{
    constexpr auto nan = std::numeric_limits<sut_t>::quiet_NaN();

    static_assert(std::isnan(primal(nan)));
}

TEST_F(jet_test_numeric_limits_t, epsilon)
{
    constexpr auto eps = std::numeric_limits<sut_t>::epsilon();

    static_assert(primal(eps) == std::numeric_limits<value_t>::epsilon());
}

// ====================================================================================================================
// Chain Rule Composition
// ====================================================================================================================

struct jet_test_chain_rule_composition_t : jet_test_t
{
    static constexpr auto eps = 1.0e-14;
};

TEST_F(jet_test_chain_rule_composition_t, exp_log)
{
    // exp(log(x)) = x

    auto const actual = exp(log(x));

    EXPECT_NEAR(x.f, actual.f, eps);
    EXPECT_NEAR(x.df, actual.df, eps);
}

TEST_F(jet_test_chain_rule_composition_t, sqrt_square)
{
    // sqrt(x^2) = |x| for x > 0
    auto const actual = sqrt(pow(x, 2.0));

    EXPECT_NEAR(actual.f, x.f, eps);
    EXPECT_NEAR(actual.df, x.df, eps);
}

TEST_F(jet_test_chain_rule_composition_t, tanh_exp)
{
    /*
        exp(a) = {exp(a.f), exp(a.f)*a.df}
        tanh(b) = {tanh(b.f), (1 - tanh(b.f)^2)*b.df}
        tanh(exp(x)) = {tanh(exp(x.f)), (1 - tanh(exp(x.f))^2)*exp(x.f)*x.df}
    */
    auto const actual = tanh(exp(x));

    auto const exp_f      = exp(x.f);
    auto const tanh_exp_f = tanh(exp_f);
    EXPECT_NEAR(actual.f, tanh_exp_f, eps);
    EXPECT_NEAR(actual.df, (1.0 - tanh_exp_f * tanh_exp_f) * exp_f * x.df, eps);
}

TEST_F(jet_test_chain_rule_composition_t, log_sqrt)
{
    /*
        sqrt(a) = {sqrt(a.f), a.df/(2*sqrt(a.f))}
        log(b) = {log(b.f), b.df/b.f}
        log(sqrt(x)) = {log(sqrt(x.f)), (x.df/(2*sqrt(x.f)))/sqrt(x.f)}
                     = {log(sqrt(x.f)), x.df/(2*sqrt(x.f)*sqrt(x.f))}
    */
    auto const actual = log(sqrt(x));

    auto const sqrt_f     = sqrt(x.f);
    auto const log_sqrt_f = log(sqrt_f);
    EXPECT_NEAR(actual.f, log_sqrt_f, eps);
    EXPECT_NEAR(actual.df, x.df / (2 * sqrt_f * sqrt_f), eps);
}

// ====================================================================================================================
// Assertion Death Tests
// ====================================================================================================================

#if !defined NDEBUG

struct jet_death_test_t : jet_test_t
{};

TEST_F(jet_death_test_t, log_negative_domain)
{
    EXPECT_DEBUG_DEATH(log(jet_t{-1.0, 1.0}), "domain error");
}

TEST_F(jet_death_test_t, log1p_negative_domain)
{
    EXPECT_DEBUG_DEATH(log1p(jet_t{-2.0, 1.0}), "domain error");
}

TEST_F(jet_death_test_t, pow_jet_to_element_negative_base)
{
    EXPECT_DEBUG_DEATH(pow(jet_t{-0.5, 1.0}, 2.5), "domain error");
}

TEST_F(jet_death_test_t, pow_jet_to_element_negative_integer_base)
{
    EXPECT_DEBUG_DEATH(pow(jet_t{-1.0, 1.0}, 2.5), "domain error");
}

TEST_F(jet_death_test_t, pow_element_to_jet_negative_base)
{
    EXPECT_DEBUG_DEATH(pow(-2.0, jet_t{1.0, 1.0}), "domain error");
}

TEST_F(jet_death_test_t, pow_jet_to_jet_negative_base)
{
    EXPECT_DEBUG_DEATH(pow(jet_t{-1.0, 1.0}, jet_t{2.0, 0.0}), "domain error");
}

TEST_F(jet_death_test_t, sqrt_negative)
{
    EXPECT_DEBUG_DEATH(sqrt(jet_t{-1.0, 1.0}), "domain error");
}

#endif

} // namespace
} // namespace crv
