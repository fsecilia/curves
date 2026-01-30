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

    EXPECT_EQ(sut.f, 0.0);
    EXPECT_EQ(sut.df, 0.0);
}

TEST_F(jet_test_construction_t, scalar)
{
    constexpr auto sut = jet_t{f};

    EXPECT_EQ(sut.f, f);
    EXPECT_EQ(sut.df, 0.0);
}

TEST_F(jet_test_construction_t, nested_scalar)
{
    constexpr auto sut = jet_t<jet_t<double>>{x};

    EXPECT_EQ(sut.f, x);
    EXPECT_EQ(sut.df, jet_t{0.0});
}

TEST_F(jet_test_construction_t, pair)
{
    EXPECT_EQ(x.f, f);
    EXPECT_EQ(x.df, df);
}

TEST_F(jet_test_construction_t, broadcast)
{
    constexpr auto sut = jet_t<jet_t<double>>{f};

    EXPECT_EQ(sut.f, jet_t{f});
    EXPECT_EQ(sut.df, jet_t{0.0});
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

    EXPECT_DOUBLE_EQ(primal(sut), static_cast<scalar_t>(f_int));
    EXPECT_DOUBLE_EQ(derivative(sut), static_cast<scalar_t>(df_int));
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
    EXPECT_TRUE(static_cast<bool>(sut_t{1.0, 0.0}));
    EXPECT_TRUE(static_cast<bool>(sut_t{-1.0, 0.0}));
    EXPECT_TRUE(static_cast<bool>(sut_t{0.001, 0.0}));
    EXPECT_TRUE(static_cast<bool>(sut_t{1.0, 999.0}));
}

TEST_F(jet_test_conversion_t, to_bool_false)
{
    EXPECT_FALSE(static_cast<bool>(sut_t{0.0, 0.0}));
    EXPECT_FALSE(static_cast<bool>(sut_t{0.0, 999.0}));
}

// ====================================================================================================================
// Accessors
// ====================================================================================================================

struct jet_test_accessors_t : jet_test_t
{};

TEST_F(jet_test_construction_t, primal)
{
    EXPECT_EQ(x.f, primal(x));
}

TEST_F(jet_test_construction_t, derivative)
{
    EXPECT_EQ(x.df, derivative(x));
}

// ====================================================================================================================
// Comparison
// ====================================================================================================================

struct jet_test_comparison_t : jet_test_t
{};

TEST_F(jet_test_comparison_t, element_equality)
{
    // equal only if primal matches AND derivative is zero
    EXPECT_TRUE((jet_t{5.0, 0.0} == 5.0));
    EXPECT_FALSE((jet_t{5.0, 1.0} == 5.0));
    EXPECT_FALSE((jet_t{5.1, 0.0} == 5.0));
}

TEST_F(jet_test_comparison_t, element_ordering)
{
    EXPECT_TRUE((jet_t{3.0, 999.0} != 4.0));
    EXPECT_TRUE((jet_t{3.0, 999.0} < 4.0));

    EXPECT_TRUE((jet_t{5.0, 999.0} != 4.0));
    EXPECT_TRUE((jet_t{5.0, 999.0} > 4.0));

    EXPECT_TRUE((jet_t{4.0, 999.0} != 4.0));
    EXPECT_TRUE((jet_t{4.0, 999.0} <= 4.0));
    EXPECT_TRUE((jet_t{4.0, 999.0} >= 4.0));
}

TEST_F(jet_test_comparison_t, jet_equality)
{
    EXPECT_TRUE((jet_t{3.0, 2.0} == jet_t{3.0, 2.0}));
    EXPECT_FALSE((jet_t{3.0, 2.0} == jet_t{3.0, 3.0}));
    EXPECT_FALSE((jet_t{3.0, 2.0} == jet_t{4.0, 2.0}));
}

TEST_F(jet_test_comparison_t, jet_ordering)
{
    EXPECT_TRUE((jet_t{3.0, 999.0} != jet_t{4.0, 0.0}));
    EXPECT_TRUE((jet_t{3.0, 999.0} < jet_t{4.0, 0.0}));

    EXPECT_TRUE((jet_t{5.0, 0.0} != jet_t{4.0, 999.0}));
    EXPECT_TRUE((jet_t{5.0, 0.0} > jet_t{4.0, 999.0}));

    EXPECT_TRUE((jet_t{4.0, 1.0} != jet_t{4.0, 2.0}));
    EXPECT_TRUE((jet_t{4.0, 1.0} <= jet_t{4.0, 2.0}));
    EXPECT_TRUE((jet_t{4.0, 2.0} >= jet_t{4.0, 1.0}));
}

} // namespace
} // namespace crv
