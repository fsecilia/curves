// SPDX-License-Identifier: MIT
/**
    \file
    \copyright Copyright (C) 2026 Frank Secilia
*/

#include <crv/math/jet/jet.hpp>
#include <crv/test/test.hpp>

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
    constexpr auto sut = +x;

    static_assert(primal(sut) == primal(x));
    static_assert(derivative(sut) == derivative(x));
}

TEST_F(jet_test_unary_arithmetic_t, minus)
{
    constexpr auto sut = -x;

    static_assert(primal(sut) == -primal(x));
    static_assert(derivative(sut) == -derivative(x));
}

// ====================================================================================================================
// Scalar Arithmetic
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
// Addition
// --------------------------------------------------------------------------------------------------------------------

struct jet_test_scalar_addition_t : jet_test_t
{};

TEST_F(jet_test_scalar_addition_t, compound_assign)
{
    auto sut = sut_t{3.0, 5.0};

    EXPECT_EQ(&sut, &(sut += 1.5));
    EXPECT_EQ(sut, (sut_t{4.5, 5.0}));
}

TEST_F(jet_test_scalar_addition_t, jet_plus_scalar)
{
    constexpr auto sut = sut_t{3.0, 5.0} + 1.5;

    static_assert(sut == sut_t{4.5, 5.0});
}

TEST_F(jet_test_scalar_addition_t, scalar_plus_det)
{
    constexpr auto sut = 1.5 + sut_t{3.0, 5.0};

    static_assert(sut == sut_t{4.5, 5.0});
}

} // namespace
} // namespace crv
