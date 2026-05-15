// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "polynomial.hpp"
#include <crv/math/jet/jet.hpp>
#include <crv/test/test.hpp>
#include <string>

namespace crv {
namespace {

// ====================================================================================================================
// polynomial_t
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
// numeric boundaries
// --------------------------------------------------------------------------------------------------------------------

namespace numeric_boundary_tests {

// default construction
static_assert(polynomial_t<int_t, 0>{}(int_t{100}) == 0);
static_assert(polynomial_t<int_t, 3>{}(int_t{100}) == 0);

// base behavior
static_assert(polynomial_t{0, 0, 0, 0}(100) == 0);
static_assert(polynomial_t{0, 0, 0, 5}(-50) == 5);

// y-intercept, t = 0
static_assert(polynomial_t{3, 5, 7, 11}(0) == 11);
static_assert(polynomial_t{-3, 5, -7, 11}(0) == 11);

// identity (t = 1 is the sum of coefficients)
static_assert(polynomial_t{3, 5, -7, 11}(int_t{1}) == 3 + 5 + -7 + 11);

// alternating identity (t = -1)
static_assert(polynomial_t{3, 5, 7, 11}(int_t{-1}) == -3 + 5 + -7 + 11);

// internal zeros bypass degrees
static_assert(polynomial_t{-3, 0, -7, 11}(2) == (-3 * 2 * 2 + -7) * 2 + 11);
static_assert(polynomial_t{-3, 5, 0, 11}(2) == (-3 * 2 + 5) * 2 * 2 + 11);

// full evaluation
static_assert(polynomial_t{3, 5, 7, 11}(2) == ((3 * 2 + 5) * 2 + 7) * 2 + 11);
static_assert(polynomial_t{-3, 5, -7, 11}(-2) == ((-3 * -2 + 5) * -2 + -7) * -2 + 11);

// varying degrees
static_assert(polynomial_t{int_t{7}}(int_t{100}) == 7); // degree 0
static_assert(polynomial_t{int_t{5}, int_t{7}}(int_t{10}) == 5 * 10 + 7); // degree 1
static_assert(polynomial_t{int_t{3}, int_t{5}, int_t{7}}(int_t{10}) == (3 * 10 + 5) * 10 + 7); // degree 2

} // namespace numeric_boundary_tests

// --------------------------------------------------------------------------------------------------------------------
// type promotion
// --------------------------------------------------------------------------------------------------------------------

namespace type_promotion_tests {

// coeffs are 8-bit, evaluation t is 16-bit
static_assert(polynomial_t{int8_t{3}, int8_t{5}, int8_t{7}}(int16_t{100}) == int16_t{30507});

// coeffs are integer, evaluation t is float
//
// LET THIS REMAIN AS A WARNING!
//
// There is no reasonable way to mix ints and floats if we want to be able to mix jets and floats.
// Either the casting required pessimizes jets, turning `jet + scalar` to `jet + jet{scalar}`, or the amount of
// metaprogramming required exceeds the benefit. Our pragmatic conclusion is to disallow mixing ints and floats in
// equations where we will be mixing jets and floats.
//
// This was the first of them we found.
//
// static_assert(polynomial_t{int_t{3}, int_t{5}, int_t{7}}(0.5) == (3 * 0.5 + 5) * 0.5 + 7);

} // namespace type_promotion_tests

// --------------------------------------------------------------------------------------------------------------------
// jet evaluation
// --------------------------------------------------------------------------------------------------------------------

namespace jet_evaluation {

// p(t) = -3t^2 + 5t + -7 = -3*((1.5)^2) + 5*(1.5) - 7 = −6.75 + 7.5 - 7 = -6.25
// p'(t) = -6t*dt + 5dt = -6(1.5)*(-3.1) + 5(-3.1) = 27.9 + −15.5 = 12.4
static_assert(polynomial_t{int_t{-3}, int_t{5}, int_t{-7}}(jet_t{1.5, -3.1}) == jet_t{-6.25, 12.4});

} // namespace jet_evaluation

// --------------------------------------------------------------------------------------------------------------------
// symbolic evaluation
// --------------------------------------------------------------------------------------------------------------------

// These test that it is actually applying Horner's rule.
namespace symbolic_evaluation_tests {

using namespace std::literals;

struct scalar_t
{
    std::string expression;

    friend constexpr auto operator+(scalar_t const& lhs, scalar_t const& rhs) -> scalar_t
    {
        return scalar_t{"(" + lhs.expression + " + " + rhs.expression + ")"};
    }

    friend constexpr auto operator*(scalar_t const& lhs, scalar_t const& rhs) -> scalar_t
    {
        return scalar_t{lhs.expression + "*" + rhs.expression};
    }

    constexpr auto operator==(scalar_t const&) const noexcept -> bool = default;

    // friend auto operator<<(std::ostream& out, scalar_t const& src) -> std::ostream& { return out << src.expression; }
};

static_assert(polynomial_t{scalar_t{"a"}}(scalar_t{"t"}) == scalar_t{"a"});
static_assert(polynomial_t{scalar_t{"a"}, scalar_t{"b"}}(scalar_t{"t"}) == scalar_t{"(a*t + b)"});
static_assert(
    polynomial_t{scalar_t{"a"}, scalar_t{"b"}, scalar_t{"c"}}(scalar_t{"t"}) == scalar_t{"((a*t + b)*t + c)"});
static_assert(polynomial_t{scalar_t{"a"}, scalar_t{"b"}, scalar_t{"c"}, scalar_t{"d"}}(scalar_t{"t"})
    == scalar_t{"(((a*t + b)*t + c)*t + d)"});

} // namespace symbolic_evaluation_tests

// ====================================================================================================================
// hermite_converter_t
// ====================================================================================================================

namespace hermite_converter_tests {

using scalar_t = float_t;
using jet_t = jet_t<scalar_t>;
using cubic_t = cubic_t<scalar_t>;
using sut_t = hermite_converter_t<scalar_t>;

constexpr auto sut = sut_t{};

// zero baseline
static_assert(cubic_t{0, 0, 0, 0} == sut(jet_t{0.0, 0.0}, jet_t{0.0, 0.0}));

//
// single-input isolation
//

// p0 = 1.0 (dp = -1.0)
// a = -2(-1) = 2, b = 3(-1) = -3, c = 0, d = 1
static_assert(cubic_t{2.0, -3.0, 0.0, 1.0} == sut(jet_t{1.0, 0.0}, jet_t{0.0, 0.0}));

// p1 = 1.0 (dp = 1.0)
// a = -2(1) = -2, b = 3(1) = 3, c = 0, d = 0
static_assert(cubic_t{-2.0, 3.0, 0, 0} == sut(jet_t{0.0, 0.0}, jet_t{1.0, 0.0}));

// m0 = 1.0
// a = 1, b = -2, c = 1, d = 0
static_assert(cubic_t{1.0, -2.0, 1.0, 0} == sut(jet_t{0.0, 1.0}, jet_t{0.0, 0.0}));

// m1 = 1.0
// a = 1, b = -1, c = 0, d = 0
static_assert(cubic_t{1.0, -1.0, 0, 0} == sut(jet_t{0.0, 0.0}, jet_t{0.0, 1.0}));

//
// combined inputs
//

// dp = 0, tangent-only: p0 = p1 = 1.0, m0 = 1.0, m1 = -1.0
// a = 0 + 1 + (-1) = 0, b = 0 - 2 - (-1) = -1, c = 1, d = 1
static_assert(cubic_t{0, -1.0, 1.0, 1.0} == sut(jet_t{1.0, 1.0}, jet_t{1.0, -1.0}));

// negative dp with all inputs active: p0 = 2, m0 = 0.5, p1 = -1, m1 = -0.5 (dp = -3)
// a = 6 + 0.5 + (-0.5) = 6, b = -9 - 1 - (-0.5) = -9.5, c = 0.5, d = 2
static_assert(cubic_t{6.0, -9.5, 0.5, 2.0} == sut(jet_t{2.0, 0.5}, jet_t{-1.0, -0.5}));

//
// linear inputs (cubic and quadratic vanish)
//

// y = x: p0 = 0, m0 = 1, p1 = 1, m1 = 1
static_assert(cubic_t{0, 0, 1.0, 0} == sut(jet_t{0.0, 1.0}, jet_t{1.0, 1.0}));

// y = 2x + 1: p0 = 1, m0 = 2, p1 = 3, m1 = 2
static_assert(cubic_t{0, 0, 2.0, 1.0} == sut(jet_t{1.0, 2.0}, jet_t{3.0, 2.0}));

// mixed fractional and negative
//
// p0 = 0.5, m0 = -0.5, p1 = -0.5, m1 = 0.5 (dp = -1)
// a = 2 + (-0.5) + 0.5 = 2, b = -3 + 1 - 0.5 = -2.5, c = -0.5, d = 0.5
static_assert(cubic_t{2.0, -2.5, -0.5, 0.5} == sut(jet_t{0.5, -0.5}, jet_t{-0.5, 0.5}));

// large magnitude
//
// p0 = -50, m0 = 10, p1 = 50, m1 = -10 (dp = 100)
// a = -200 + 10 + (-10) = -200, b = 300 - 20 + 10 = 290, c = 10, d = -50
static_assert(cubic_t{-200.0, 290.0, 10.0, -50.0} == sut(jet_t{-50.0, 10.0}, jet_t{50.0, -10.0}));

} // namespace hermite_converter_tests

} // namespace
} // namespace crv
