// SPDX-License-Identifier: MIT
/**
    \file
    \copyright Copyright (C) 2026 Frank Secilia
*/

#include <crv/math/jet/jet.hpp>
#include <crv/test/test.hpp>
#include <limits>
#include <ostream>

namespace crv {
namespace {

using scalar_t = real_t;
using sut_t    = jet_t<scalar_t>;

static constexpr auto eps = epsilon<scalar_t>();
static constexpr auto inf = infinity<scalar_t>();
static constexpr auto nan = std::numeric_limits<scalar_t>::quiet_NaN();

struct jet_test_t : Test
{
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

    static_assert(0.0 == sut.f);
    static_assert(0.0 == sut.df);
}

TEST_F(jet_test_construction_t, scalar)
{
    constexpr auto sut = jet_t{f};

    static_assert(f == sut.f);
    static_assert(0.0 == sut.df);
}

TEST_F(jet_test_construction_t, nested_scalar)
{
    constexpr auto sut = jet_t<jet_t<double>>{x};

    static_assert(x == sut.f);
    static_assert(jet_t{0.0} == sut.df);
}

TEST_F(jet_test_construction_t, pair)
{
    static_assert(f == x.f);
    static_assert(df == x.df);
}

TEST_F(jet_test_construction_t, broadcast_level_1)
{
    constexpr auto sut = jet_t<jet_t<double>>{f};

    static_assert(jet_t{f} == sut.f);
    static_assert(jet_t{0.0} == sut.df);
}

TEST_F(jet_test_construction_t, broadcast_level_2)
{
    constexpr auto sut = jet_t<jet_t<jet_t<double>>>{f};

    static_assert(jet_t<jet_t<double>>{f} == sut.f);
    static_assert(jet_t<jet_t<double>>{0.0} == sut.df);
}

TEST_F(jet_test_construction_t, broadcast_level_3)
{
    constexpr auto sut = jet_t<jet_t<jet_t<jet_t<double>>>>{f};

    static_assert(jet_t<jet_t<jet_t<double>>>{f} == sut.f);
    static_assert(jet_t<jet_t<jet_t<double>>>{0.0} == sut.df);
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

    static_assert(static_cast<scalar_t>(f_int) == sut.f);
    static_assert(static_cast<scalar_t>(df_int) == sut.df);
}

TEST_F(jet_test_conversion_t, assign)
{
    constexpr auto jet_int = jet_t{f_int, df_int};
    auto           sut     = sut_t{};

    sut = jet_int;

    EXPECT_DOUBLE_EQ(static_cast<scalar_t>(f_int), sut.f);
    EXPECT_DOUBLE_EQ(static_cast<scalar_t>(df_int), sut.df);
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
// Scalar Arithmetic
// ====================================================================================================================

struct scalar_op_test_vector_t
{
    std::string name;
    sut_t       jet;
    scalar_t    scalar;
    sut_t       expected;

    friend auto operator<<(std::ostream& out, scalar_op_test_vector_t const& src) -> std::ostream&
    {
        return out << src.jet << " ⨂ " << src.scalar << " = " << src.expected;
    }
};

struct jet_test_scalar_op_t : jet_test_t, WithParamInterface<scalar_op_test_vector_t>
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
scalar_op_test_vector_t const scalar_addition_vectors[] = {
    {"positve + positive", {3.0, 5.0}, 11, {14.0, 5.0}},
    {"negative + negative", {-3.0, -5.0}, -11, {-14.0, -5.0}},
    {"mixed signs", {3.0, -5.0}, -11, {-8.0, -5.0}},
    {"zeros", {0.0, 0.0}, 0.0, {0.0, 0.0}},
    {"mixed zeros", {5.0, 0.0}, 0.0, {5.0, 0.0}},
};
// clang-format on
INSTANTIATE_TEST_SUITE_P(scalar_addition, jet_test_scalar_addition_t, ValuesIn(scalar_addition_vectors),
                         test_name_generator_t<scalar_op_test_vector_t>{});

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
scalar_op_test_vector_t const scalar_subtraction_vectors[] = {
    {"positve - positive", {3.0, 5.0}, 11, {-8.0, 5.0}},
    {"negative - negative", {-3.0, -5.0}, -11, {8.0, -5.0}},
    {"mixed signs", {3.0, -5.0}, -11, {14.0, -5.0}},
    {"zeros", {0.0, 0.0}, 0.0, {0.0, 0.0}},
    {"mixed zeros", {5.0, 0.0}, 0.0, {5.0, 0.0}},
};
// clang-format on
INSTANTIATE_TEST_SUITE_P(scalar_subtraction, jet_test_scalar_subtraction_t, ValuesIn(scalar_subtraction_vectors),
                         test_name_generator_t<scalar_op_test_vector_t>{});

// --------------------------------------------------------------------------------------------------------------------
// Multiplication
// --------------------------------------------------------------------------------------------------------------------

struct jet_test_scalar_multiplication_t : jet_test_scalar_op_t
{};

TEST_P(jet_test_scalar_multiplication_t, compound_assign)
{
    auto const& actual = jet *= scalar;

    EXPECT_EQ(&jet, &actual);
    EXPECT_EQ(expected, actual);
}

TEST_P(jet_test_scalar_multiplication_t, jet_times_scalar)
{
    auto const actual = jet * scalar;

    EXPECT_EQ(expected, actual);
}

TEST_P(jet_test_scalar_multiplication_t, scalar_times_jet)
{
    auto const actual = scalar * jet;

    EXPECT_EQ(expected, actual);
}

// clang-format off
scalar_op_test_vector_t const scalar_multiplication_vectors[] = {
    {"positive*positive", {3.0, 2.0}, 5.0, {15.0, 10.0}},
    {"positive*negative", {3.0, 2.0}, -5.0, {-15.0, -10.0}},
    {"negative*positive", {-3.0, -2.0}, 5.0, {-15.0, -10.0}},
    {"negative*negative", {-3.0, -2.0}, -5.0, {15.0, 10.0}},
    {"*0", {3.0, 2.0}, 0.0, {0.0, 0.0}},
    {"0*", {0.0, 0.0}, 5.0, {0.0, 0.0}},
    {"0*0", {0.0, 0.0}, 0.0, {0.0, 0.0}},
};
// clang-format on
INSTANTIATE_TEST_SUITE_P(scalar_multiplication, jet_test_scalar_multiplication_t,
                         ValuesIn(scalar_multiplication_vectors), test_name_generator_t<scalar_op_test_vector_t>{});

// --------------------------------------------------------------------------------------------------------------------
// Division
// --------------------------------------------------------------------------------------------------------------------

struct jet_test_scalar_division_t : jet_test_scalar_op_t
{};

TEST_P(jet_test_scalar_division_t, compound_assign)
{
    auto const& actual = jet /= scalar;

    EXPECT_EQ(&jet, &actual);
    EXPECT_EQ(expected, actual);
}

TEST_P(jet_test_scalar_division_t, jet_divided_by_scalar)
{
    auto const actual = jet / scalar;

    EXPECT_EQ(expected, actual);
}

TEST_P(jet_test_scalar_division_t, scalar_divided_by_jet)
{
    auto const actual   = scalar / jet;
    auto const identity = actual * expected;

    EXPECT_NEAR(1.0, identity.f, 1e-15);
    EXPECT_NEAR(0.0, identity.df, 1e-15);
}

// clang-format off
scalar_op_test_vector_t const scalar_division_vectors[] = {
    {"positive/positive", {15.0, 10.0}, 5.0, {3.0, 2.0}},
    {"positive/negative", {15.0, 10.0}, -5.0, {-3.0, -2.0}},
    {"negative/positive", {-15.0, -10.0}, 5.0, {-3.0, -2.0}},
    {"negative/negative", {-15.0, -10.0}, -5.0, {3.0, 2.0}},
};
// clang-format on
INSTANTIATE_TEST_SUITE_P(scalar_division, jet_test_scalar_division_t, ValuesIn(scalar_division_vectors),
                         test_name_generator_t<scalar_op_test_vector_t>{});

// ====================================================================================================================
// Vector Arithmetic
// ====================================================================================================================

struct vector_op_test_vector_t
{
    std::string name;
    sut_t       lhs;
    sut_t       rhs;
    sut_t       expected;

    friend auto operator<<(std::ostream& out, vector_op_test_vector_t const& src) -> std::ostream&
    {
        return out << src.lhs << " ⨂ " << src.rhs << " = " << src.expected;
    }
};

struct jet_test_vector_op_t : jet_test_t, WithParamInterface<vector_op_test_vector_t>
{
    sut_t       lhs      = GetParam().lhs;
    sut_t const rhs      = GetParam().rhs;
    sut_t const expected = GetParam().expected;
};

// --------------------------------------------------------------------------------------------------------------------
// Addition
// --------------------------------------------------------------------------------------------------------------------

struct jet_test_vector_addition_t : jet_test_vector_op_t
{};

TEST_P(jet_test_vector_addition_t, compound_assign)
{
    auto const& actual = lhs += rhs;

    EXPECT_EQ(&lhs, &actual);
    EXPECT_EQ(expected, actual);
}

TEST_P(jet_test_vector_addition_t, binary_op)
{
    auto const actual = lhs + rhs;

    EXPECT_EQ(expected, actual);
}

TEST_P(jet_test_vector_addition_t, binary_op_commuted)
{
    auto const actual = rhs + lhs;

    EXPECT_EQ(expected, actual);
}

vector_op_test_vector_t const vector_addition_vectors[] = {
    {"positve + positive", {1.0, 2.0}, {3.0, 4.0}, {4.0, 6.0}},
    {"mixed signs", {-1.0, 2.0}, {3.0, -4.0}, {2.0, -2.0}},
    {"zeros", {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}},
    {"mixed zeros", {5.0, 0.0}, {0.0, 3.0}, {5.0, 3.0}},
};
INSTANTIATE_TEST_SUITE_P(vector_addition, jet_test_vector_addition_t, ValuesIn(vector_addition_vectors),
                         test_name_generator_t<vector_op_test_vector_t>{});

// --------------------------------------------------------------------------------------------------------------------
// Subtraction
// --------------------------------------------------------------------------------------------------------------------

struct jet_test_vector_subtraction_t : jet_test_vector_op_t
{};

TEST_P(jet_test_vector_subtraction_t, compound_assign)
{
    auto const& actual = lhs -= rhs;

    EXPECT_EQ(&lhs, &actual);
    EXPECT_EQ(expected, actual);
}

TEST_P(jet_test_vector_subtraction_t, binary_op)
{
    auto const actual = lhs - rhs;

    EXPECT_EQ(expected, actual);
}

TEST_P(jet_test_vector_subtraction_t, binary_op_commuted)
{
    auto const actual = rhs - lhs;

    EXPECT_EQ(-expected, actual);
}

vector_op_test_vector_t const vector_subtraction_vectors[] = {
    {"positve - smaller", {11.0, 17.0}, {5.0, 7.0}, {6.0, 10.0}},
    {"positve - larger", {5.0, 7.0}, {11.0, 17.0}, {-6.0, -10.0}},
    {"mixed signs", {-5.0, 7.0}, {11.0, -17.0}, {-16.0, 24.0}},
    {"zeros", {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}},
    {"mixed zeros", {5.0, 0.0}, {0.0, 7.0}, {5.0, -7.0}},
};
INSTANTIATE_TEST_SUITE_P(vector_subtraction, jet_test_vector_subtraction_t, ValuesIn(vector_subtraction_vectors),
                         test_name_generator_t<vector_op_test_vector_t>{});

// --------------------------------------------------------------------------------------------------------------------
// Multiplication
// --------------------------------------------------------------------------------------------------------------------

struct jet_test_vector_multiplication_t : jet_test_vector_op_t
{};

TEST_P(jet_test_vector_multiplication_t, compound_assign)
{
    auto const& actual = lhs *= rhs;

    EXPECT_EQ(&lhs, &actual);
    EXPECT_EQ(expected, actual);
}

TEST_P(jet_test_vector_multiplication_t, binary_op)
{
    auto const actual = lhs * rhs;

    EXPECT_EQ(expected, actual);
}

TEST_P(jet_test_vector_multiplication_t, binary_op_commuted)
{
    auto const actual = rhs * lhs;

    EXPECT_EQ(expected, actual);
}

/*
    product rule: d(u*v) = u*dv + du*v
    {f1, df1}*{f2, df2} = {f1*f2, f1*df2 + df1*f2}
*/
vector_op_test_vector_t const vector_multiplication_vectors[] = {
    // {2, 1}*{3, 1} = {2*3, 2*1 + 1*3} = {6, 5}
    {"vector*vector", {2.0, 1.0}, {3.0, 1.0}, {6.0, 5.0}},

    // {3, 2}*{5, 7} = {3*5, 3*7 + 2*5} = {15, 31}
    {"scaled_vector*scaled_vector", {3.0, 2.0}, {5.0, 7.0}, {15.0, 31.0}},

    // zero cases
    {"derivative*scaled_vector", {0.0, 2.0}, {5.0, 7.0}, {0.0, 10.0}},
    {"scaledd_vector*zero", {3.0, 2.0}, {0.0, 0.0}, {0.0, 0.0}},

    // identity
    {"identity", {3.0, 2.0}, {1.0, 0.0}, {3.0, 2.0}},
};
INSTANTIATE_TEST_SUITE_P(vector_multiplication, jet_test_vector_multiplication_t,
                         ValuesIn(vector_multiplication_vectors), test_name_generator_t<vector_op_test_vector_t>{});

// --------------------------------------------------------------------------------------------------------------------
// Division
// --------------------------------------------------------------------------------------------------------------------

struct jet_test_vector_division_t : jet_test_vector_op_t
{};

TEST_P(jet_test_vector_division_t, compound_assign)
{
    auto const& actual = lhs /= rhs;

    EXPECT_EQ(&lhs, &actual);
    EXPECT_EQ(expected, actual);
}

TEST_P(jet_test_vector_division_t, binary_op)
{
    auto const actual = lhs / rhs;

    EXPECT_EQ(expected, actual);
}

TEST_P(jet_test_vector_division_t, binary_op_inverse)
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
vector_op_test_vector_t const vector_division_vectors[] = {
    // {3, 2} / {1, 0} = {3/1, (1 - (3/2)*0)/1}
    {"scaled_vector/identity", {3.0, 2.0}, {1.0, 0.0}, {3.0, 2.0}},

    // {10, 4} / {2, 0} = {10/2, (4 - (10/2)*0)/2}
    {"scaled_vector/scalar", {10.0, 4.0}, {2.0, 0.0}, {5.0, 2.0}},

    // {6, 5} / {2, 1} = {6/2, (5 - (6/2)*1)/2}
    {"scaled_vector/vector", {6.0, 5.0}, {2.0, 1.0}, {3.0, 1.0}},
};
INSTANTIATE_TEST_SUITE_P(vector_division, jet_test_vector_division_t, ValuesIn(vector_division_vectors),
                         test_name_generator_t<vector_op_test_vector_t>{});

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
    {"infinite scalar", {inf, 0.0}, false, true, false},
    {"infinite derivative", {0.0, inf}, false, true, false},
    {"infinity", {inf, inf}, false, true, false},

    {"nan scalar", {nan, 0.0}, false, false, true},
    {"nan derivative", {0.0, nan}, false, false, true},
    {"nan", {nan, nan}, false, false, true},

    {"infinite scalar, nan derivative", {inf, nan}, false, false, true},
    {"nan scalar, infinite derivative", {nan, inf}, false, false, true},
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

// d(|x|) = sgn(x)*dx
struct jet_test_abs_t : jet_test_math_func_x_t
{};

TEST_P(jet_test_abs_t, result)
{
    auto const actual = abs(x);

    EXPECT_DOUBLE_EQ(expected.f, actual.f);
    EXPECT_DOUBLE_EQ(expected.df, actual.df);
}

// clang-format off
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

// d(copysign(x, y)) = sgn(x)*sgn(y)*dx
struct jet_test_copysign_t : jet_test_math_func_xy_t
{};

TEST_P(jet_test_copysign_t, result)
{
    auto const actual = copysign(x, y);

    EXPECT_DOUBLE_EQ(expected.f, actual.f);
    EXPECT_DOUBLE_EQ(expected.df, actual.df);
}

// clang-format off
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

// d(cos(x)) = -sin(x)*dx
struct jet_test_cos_t : jet_test_math_func_x_t
{};

TEST_P(jet_test_cos_t, result)
{
    auto const actual = cos(x);

    EXPECT_NEAR(expected.f, actual.f, eps);
    EXPECT_NEAR(expected.df, actual.df, eps);
}

// clang-format off
const math_func_test_vector_x_t cos_vectors[] = {
    {"0", {0.0, 1.3}, {1.0, 0.0}},
    {"-pi/2", {-M_PI_2, 1.3}, {0.0, 1.3}},
    {"pi/5", {M_PI / 5, 1.3}, {cos(M_PI / 5), -1.3*sin(M_PI / 5)}},
    {"pi/3", {M_PI / 3, 1.3}, {cos(M_PI / 3), -1.3*sin(M_PI / 3)}},
    {"pi/2,", {M_PI_2, 1.3}, {0.0, -1.3}},
    {"pi", {M_PI, 1.3}, {-1.0, 0.0}},
};
// clang-format on
INSTANTIATE_TEST_SUITE_P(cos, jet_test_cos_t, ValuesIn(cos_vectors),
                         test_name_generator_t<math_func_test_vector_x_t>{});

// --------------------------------------------------------------------------------------------------------------------
// exp
// --------------------------------------------------------------------------------------------------------------------

// d(exp(x)) = exp(x)*dx
struct jet_test_exp_t : jet_test_math_func_x_t
{};

TEST_P(jet_test_exp_t, result)
{
    auto const actual = exp(x);

    EXPECT_NEAR(expected.f, actual.f, 4 * eps);
    EXPECT_NEAR(expected.df, actual.df, eps);
}

// clang-format off
const math_func_test_vector_x_t exp_vectors[] = {
    {"-1", {-1.0, 1.3}, {1.0 / M_E, 1.3 / M_E}},
    {"0", {0.0, 1.3}, {1.0, 1.3}},
    {"0.5", {0.5, 1.3}, {sqrt(M_E), 1.3*sqrt(M_E)}},
    {"1", {1.0, 1.3}, {M_E, 1.3*M_E}},
    {"2", {2.0, 1.3}, {M_E*M_E, M_E*M_E*1.3}},
};
// clang-format on
INSTANTIATE_TEST_SUITE_P(exp, jet_test_exp_t, ValuesIn(exp_vectors),
                         test_name_generator_t<math_func_test_vector_x_t>{});

// --------------------------------------------------------------------------------------------------------------------
// hypot
// --------------------------------------------------------------------------------------------------------------------

// d(hypot(x, y)) = (x*dx + y*dy) / hypot(x, y)
struct jet_test_hypot_t : jet_test_math_func_xy_t
{};

TEST_P(jet_test_hypot_t, result)
{
    auto const actual = hypot(x, y);

    EXPECT_DOUBLE_EQ(expected.f, actual.f);
    EXPECT_DOUBLE_EQ(expected.df, actual.df);
}

// clang-format off
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

// d(log(x)) = dx/x
struct jet_test_log_t : jet_test_math_func_x_t
{};

TEST_P(jet_test_log_t, result)
{
    auto const actual = log(x);

    EXPECT_NEAR(expected.f, actual.f, eps);
    EXPECT_NEAR(expected.df, actual.df, eps);
}

// clang-format off
const math_func_test_vector_x_t log_vectors[] = {
    {"0.5", {0.5, 1.3}, {log(0.5), 1.3/0.5}},
    {"1", {1.0, 1.3}, {0.0, 1.3}},
    {"e", {M_E, 1.3}, {1.0, 1.3/M_E}},
    {"2", {2.0, 1.3}, {log(2), 1.3/2}},
    {"10", {10.0, 1.3}, {log(10), 1.3/10}},
};
// clang-format on
INSTANTIATE_TEST_SUITE_P(log, jet_test_log_t, ValuesIn(log_vectors),
                         test_name_generator_t<math_func_test_vector_x_t>{});

// ====================================================================================================================
// Assertion Death Tests
// ====================================================================================================================

#if !defined NDEBUG

struct jet_death_test_t : jet_test_t
{};

TEST_F(jet_death_test_t, log_negative_domain)
{
    EXPECT_DEBUG_DEATH(log(jet_t{-1.0, 1.0}), "jet_t::log: domain error");
}

#endif

} // namespace
} // namespace crv
