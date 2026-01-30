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

TEST_F(jet_test_t, arithmetic)
{
    static_assert(arithmetic<int_t>);
    static_assert(arithmetic<real_t>);

    struct nonarithmetic_t;
    static_assert(!arithmetic<nonarithmetic_t>);
}

// ====================================================================================================================
// Scalar Fallbacks
// ====================================================================================================================

TEST_F(jet_test_t, primal)
{
    static_assert(f == primal(f));
    static_assert(-f == primal(-f));
    static_assert(df == primal(df));
}

TEST_F(jet_test_t, derivative)
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

} // namespace
} // namespace crv
