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

    static constexpr scalar_t f  = 37.2; // arbitrary
    static constexpr scalar_t df = 26.3; // arbitrary
};

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

} // namespace
} // namespace crv
