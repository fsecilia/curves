// SPDX-License-Identifier: MIT
/**
    \file
    \brief tests jet composition over jets

    These tests verify that autodiff composes correctly to calc second derivatives via jet_t<jet_t<double>>.

    \copyright Copyright (C) 2026 Frank Secilia
*/

#include <crv/math/jet/jet.hpp>
#include <crv/test/test.hpp>

namespace crv {
namespace {

struct nested_jet_test_t : Test
{
    using scalar_t = double;
    using value_t  = jet_t<scalar_t>;
    using sut_t    = jet_t<value_t>;

    // seed with primish numbers
    static constexpr scalar_t s{1.3};
    static constexpr value_t  v{1.7, 1.9};
    static constexpr sut_t    x{{2.3, 3.1}, {5.3, 7.1}};
    static constexpr sut_t    y{{5.9, 7.3}, {8.3, 9.7}};

    static constexpr scalar_t eps = 1e-10;

    constexpr auto compare(sut_t const& expected, sut_t const& actual, scalar_t tolerance = eps) noexcept -> void
    {
        EXPECT_NEAR(expected.f.f, actual.f.f, eps);
        EXPECT_NEAR(expected.f.df, actual.f.df, eps);
        EXPECT_NEAR(expected.df.f, actual.df.f, eps);
        EXPECT_NEAR(expected.df.df, actual.df.df, eps);
    }
};

// --------------------------------------------------------------------------------------------------------------------
// Construction
// --------------------------------------------------------------------------------------------------------------------

struct nested_jet_test_construction_t : nested_jet_test_t
{};

TEST_F(nested_jet_test_construction_t, value)
{
    constexpr auto sut = jet_t<jet_t<double>>{v};

    static_assert(v == sut.f);
    static_assert(jet_t{0.0} == sut.df);
}

TEST_F(nested_jet_test_construction_t, scalar_level_1)
{
    constexpr auto sut = jet_t<jet_t<double>>{s};

    static_assert(jet_t{s} == sut.f);
    static_assert(jet_t{0.0} == sut.df);
}

TEST_F(nested_jet_test_construction_t, scalar_level_2)
{
    constexpr auto sut = jet_t<jet_t<jet_t<double>>>{s};

    static_assert(jet_t<jet_t<double>>{s} == sut.f);
    static_assert(jet_t<jet_t<double>>{0.0} == sut.df);
}

TEST_F(nested_jet_test_construction_t, scalar_level_3)
{
    constexpr auto sut = jet_t<jet_t<jet_t<jet_t<double>>>>{s};

    static_assert(jet_t<jet_t<jet_t<double>>>{s} == sut.f);
    static_assert(jet_t<jet_t<jet_t<double>>>{0.0} == sut.df);
}

// --------------------------------------------------------------------------------------------------------------------
// Arithmetic
// --------------------------------------------------------------------------------------------------------------------

struct nested_jet_test_arithmetic_t : nested_jet_test_t
{};

TEST_F(nested_jet_test_arithmetic_t, compound_plus_scalar)
{
    auto const expected = sut_t{{x.f.f + s, x.f.df}, x.df};

    auto        sut    = x;
    auto const& actual = sut += s;

    ASSERT_EQ(&sut, &actual);
    ASSERT_EQ(expected, actual);
}

TEST_F(nested_jet_test_arithmetic_t, jet_plus_scalar)
{
    auto const expected = sut_t{{x.f.f + s, x.f.df}, x.df};

    auto const actual = x + s;

    ASSERT_EQ(expected, actual);
}

TEST_F(nested_jet_test_arithmetic_t, scalar_plus_jet)
{
    auto const expected = sut_t{{s + x.f.f, x.f.df}, x.df};

    auto const actual = s + x;

    ASSERT_EQ(expected, actual);
}

TEST_F(nested_jet_test_arithmetic_t, compound_minus_scalar)
{
    auto const expected = sut_t{{x.f.f - s, x.f.df}, x.df};

    auto        sut    = x;
    auto const& actual = sut -= s;

    ASSERT_EQ(&sut, &actual);
    ASSERT_EQ(expected, actual);
}

TEST_F(nested_jet_test_arithmetic_t, jet_minus_scalar)
{
    auto const expected = sut_t{{x.f.f - s, x.f.df}, x.df};

    auto const actual = x - s;

    ASSERT_EQ(expected, actual);
}

TEST_F(nested_jet_test_arithmetic_t, scalar_minus_jet)
{
    auto const expected = sut_t{{s - x.f.f, -x.f.df}, -x.df};

    auto const actual = s - x;

    ASSERT_EQ(expected, actual);
}

TEST_F(nested_jet_test_arithmetic_t, compound_times_scalar)
{
    auto const expected = sut_t{x.f * s, x.df * s};

    auto        sut    = x;
    auto const& actual = sut *= s;

    ASSERT_EQ(&sut, &actual);
    ASSERT_EQ(expected, actual);
}

TEST_F(nested_jet_test_arithmetic_t, jet_times_scalar)
{
    auto const expected = sut_t{x.f * s, x.df * s};

    auto const actual = x * s;

    ASSERT_EQ(expected, actual);
}

TEST_F(nested_jet_test_arithmetic_t, scalar_times_jet)
{
    auto const expected = sut_t{x.f * s, x.df * s};

    auto const actual = s * x;

    ASSERT_EQ(expected, actual);
}

TEST_F(nested_jet_test_arithmetic_t, compound_over_scalar)
{
    auto const expected = sut_t{x.f / s, x.df / s};

    auto        sut    = x;
    auto const& actual = sut /= s;

    ASSERT_EQ(&sut, &actual);
    ASSERT_EQ(expected, actual);
}

TEST_F(nested_jet_test_arithmetic_t, jet_over_scalar)
{
    auto const expected = sut_t{x.f / s, x.df / s};

    auto const actual = x / s;

    ASSERT_EQ(expected, actual);
}

TEST_F(nested_jet_test_arithmetic_t, scalar_over_jet)
{
    auto const expected = sut_t{s / x.f, -s * x.df / (x.f * x.f)};

    auto const actual = s / x;

    compare(expected, actual);
}

TEST_F(nested_jet_test_arithmetic_t, mixed_linear_combination)
{
    /*
        f(x) = 3x^2 + 2x + v + s
        f'(x) = 6x*dx + 2*dx
    */
    auto const expected_primal     = 3.0 * x.f * x.f + 2.0 * x.f + v + s;
    auto const expected_derivative = 6.0 * x.f * x.df + 2.0 * x.df;

    auto const actual = 3.0 * x * x + 2.0 * x + v + s;

    compare(actual, {expected_primal, expected_derivative});
}

TEST_F(nested_jet_test_arithmetic_t, quartic)
{
    /*
        f(x) = x^4
        f'(x) = 4x^3*dx
    */
    auto const expected_primal     = x.f * x.f * x.f * x.f;
    auto const expected_derivative = 4.0 * x.f * x.f * x.f * x.df;

    auto const x2     = x * x;
    auto const x4     = x2 * x2;
    auto const actual = x2 * x2;

    compare(actual, {expected_primal, expected_derivative});
}

// --------------------------------------------------------------------------------------------------------------------
// Second Derivative
// --------------------------------------------------------------------------------------------------------------------

struct nested_jet_second_derivative_t : nested_jet_test_t
{
    static constexpr auto u        = x.f.f;   // scalar value
    static constexpr auto du_inner = x.f.df;  // derivative wrt inner variable
    static constexpr auto du_outer = x.df.f;  // derivative wrt outer variable
    static constexpr auto d2u      = x.df.df; // cross derivative

    static constexpr auto v        = y.f.f;
    static constexpr auto dv_inner = y.f.df;
    static constexpr auto dv_outer = y.df.f;
    static constexpr auto d2v      = y.df.df;
};

TEST_F(nested_jet_second_derivative_t, cos)
{
    /*
        cos({{u, du_inner}, {du_outer, d2u}}) = {cos({u, du_inner}), -sin({u, du_inner})*{du_outer, d2u}}
            = {cos({u, du_inner}), -{sin(u), cos(u)*du_inner}*{du_outer, d2u}}
            = {cos({u, du_inner}), -cos(u)*du_inner*du_outer - sin(u)*d2u}}
    */
    auto const expected = -cos(u) * du_inner * du_outer - sin(u) * d2u;

    auto const actual = cos(x);

    EXPECT_NEAR(expected, actual.df.df, eps);
}

TEST_F(nested_jet_second_derivative_t, exp)
{
    /*
        exp({{u, du_inner}, {du_outer, d2u}}) = {exp({u, du_inner}), exp({u, du_inner})*{du_outer, d2u}}
            = {{exp(u), exp(u)*du_inner}, {exp(u), exp(u)*du_inner}*{du_outer, d2u}}
            = {{exp(u), exp(u)*du_inner}, {exp(u)*du_outer, exp(u)*d2u + exp(u)*du_inner*du_outer}}
    */
    auto const expected = exp(u) * (du_inner * du_outer + d2u);

    auto const actual = exp(x);

    EXPECT_NEAR(expected, actual.df.df, eps);
}

TEST_F(nested_jet_second_derivative_t, pow_decomposed_into_values)
{
    // decompose pow by values directly; this unnests one level - each term here is a 1-jet
    auto const expected = sut_t{pow(x.f, y.f), pow(x.f, y.f) * log(x.f) * y.df + pow(x.f, y.f - 1) * y.f * x.df};

    auto const actual = pow(x, y);

    compare(expected, actual);
}

/*
    test second derivative of pow decomposed all the way to scalars

    This test verifies the full scalar expansion of the second derivative. Just writing the final expression out has a
    lot of terms, many of which are repeated. It is large enough to be opaque. This test tries to document some of the
    terms with meaningful names.
*/
TEST_F(nested_jet_second_derivative_t, pow_decomposed_into_scalars)
{
    // powers of base
    auto const f    = pow(u, v);     // u^v
    auto const f_1  = pow(u, v - 1); // u^(v - 1), first derivative factor
    auto const f_2  = pow(u, v - 2); // u^(v - 2), second derivative factor
    auto const ln_u = log(u);

    /*
        first partial derivatives

        Define psi as the sensitivity combining both input's contributions:

            d(u^v) = u^(v - 1)*(u*ln(u)*dv + v*du)
            psi   := (u*ln(u)*dv + v*du)
            d(u^v) = f_1*psi
    */

    auto const psi_t = u * ln_u * dv_inner + v * du_inner; // wrt inner variable t
    auto const psi_s = u * ln_u * dv_outer + v * du_outer; // wrt outer variable s

    auto const df_dt = f_1 * psi_t;
    auto const df_ds = f_1 * psi_s;

    /*
        second mixed partial derivatives

        The second mixed partial is `∂/∂s[f_1*psi_t]`
        Applying the product rule gives `(∂f_1/∂s)*psi_t + f_1*(∂psi_t/∂s)`
    */

    // term 1: (∂f_1/∂s)*psi_t
    // ∂(u^(v - 1))/∂s = u^(v - 2) * (u*ln(u)*dv_s + (v-1)*du_s)
    auto const psi_s_shifted = u * ln_u * dv_outer + (v - 1) * du_outer; // psi for exponent v-1
    auto const term1         = f_2 * psi_s_shifted * psi_t;

    // term 2: f_1*(∂psi_t/∂s)
    // ∂psi_t/∂s = ∂(u*ln(u)*dv_t + v*du_t)/∂s
    //           = (ln(u) + 1)*du_s*dv_t + u*ln(u)*d²v + dv_s*du_t + v*d²u
    auto const d_uln_ds  = (ln_u + 1) * du_outer; // d(u*ln(u))/ds
    auto const dpsi_t_ds = d_uln_ds * dv_inner    // from u*ln(u) term
                           + u * ln_u * d2v       // dv_t -> d²v
                           + dv_outer * du_inner  // cross partial dv*du
                           + v * d2u;             // du_t -> d²u
    auto const term2 = f_1 * dpsi_t_ds;

    auto const d2f_dsdt = term1 + term2;

    /*
        assemble nested jet
    */

    auto const expected = sut_t{
        {f, df_dt},       // {primal, ∂/∂t}
        {df_ds, d2f_dsdt} // {∂/∂s, ∂²/∂s∂t}
    };

    compare(expected, pow(x, y));
}

// ====================================================================================================================
// Scalar Ambiguity Resolution
// ====================================================================================================================

template <typename jet_t> struct jet_test_nested_scalar_ambiguity_resolution_t : nested_jet_test_t
{};

using jet_types_t = Types<jet_t<double>, jet_t<jet_t<double>>, jet_t<jet_t<jet_t<double>>>>;
TYPED_TEST_SUITE(jet_test_nested_scalar_ambiguity_resolution_t, jet_types_t);

TYPED_TEST(jet_test_nested_scalar_ambiguity_resolution_t, ordering)
{
    ASSERT_LT(TypeParam{3.0}, 5.0);
    ASSERT_LT(3.0, TypeParam{5.0});
}

TYPED_TEST(jet_test_nested_scalar_ambiguity_resolution_t, equality)
{
    ASSERT_EQ(TypeParam{3.0}, 3.0);
    ASSERT_EQ(3.0, TypeParam{3.0});
}

TYPED_TEST(jet_test_nested_scalar_ambiguity_resolution_t, compound_addition)
{
    auto nested = TypeParam{3.0};
    ASSERT_EQ(nested += 5.0, TypeParam{8.0});
}

TYPED_TEST(jet_test_nested_scalar_ambiguity_resolution_t, addition)
{
    ASSERT_EQ(TypeParam{3.0} + 5.0, TypeParam{8.0});
    ASSERT_EQ(3.0 + TypeParam{5.0}, TypeParam{8.0});
}

TYPED_TEST(jet_test_nested_scalar_ambiguity_resolution_t, compound_subtraction)
{
    auto nested = TypeParam{3.0};
    ASSERT_EQ(nested -= 5.0, TypeParam{-2.0});
}

TYPED_TEST(jet_test_nested_scalar_ambiguity_resolution_t, subtraction)
{
    ASSERT_EQ(TypeParam{3.0} - 5.0, TypeParam{-2.0});
    ASSERT_EQ(3.0 - TypeParam{5.0}, TypeParam{-2.0});
}

TYPED_TEST(jet_test_nested_scalar_ambiguity_resolution_t, compound_multiplication)
{
    auto nested = TypeParam{3.0};
    ASSERT_EQ(nested *= 5.0, TypeParam{15.0});
}

TYPED_TEST(jet_test_nested_scalar_ambiguity_resolution_t, multiplication)
{
    ASSERT_EQ(TypeParam{3.0} * 5.0, TypeParam{15.0});
    ASSERT_EQ(3.0 * TypeParam{5.0}, TypeParam{15.0});
}

TYPED_TEST(jet_test_nested_scalar_ambiguity_resolution_t, compound_division)
{
    auto nested = TypeParam{16.0};
    ASSERT_EQ(nested /= 8.0, TypeParam{2.0});
}

TYPED_TEST(jet_test_nested_scalar_ambiguity_resolution_t, division)
{
    ASSERT_EQ(TypeParam{16.0} / 8.0, TypeParam{2.0});
    ASSERT_EQ(16.0 / TypeParam{8.0}, TypeParam{2.0});
}

TYPED_TEST(jet_test_nested_scalar_ambiguity_resolution_t, pow)
{
    ASSERT_EQ(pow(TypeParam{2.0}, 3.0), TypeParam{8.0});
    ASSERT_EQ(pow(2.0, TypeParam{3.0}), TypeParam{8.0});
}

} // namespace
} // namespace crv
