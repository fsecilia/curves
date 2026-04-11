// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "bisector.hpp"
#include <crv/test/test.hpp>

namespace crv::quadrature {
namespace {

constexpr auto initial_tolerance = 7.65; // arbitrary

// --------------------------------------------------------------------------------------------------------------------
// fixtures
// --------------------------------------------------------------------------------------------------------------------

// simple trapezoidal rule that reports a fake internal error
template <std::floating_point real_t> struct rule_t
{
    struct estimate_t
    {
        real_t sum;
        real_t error;
    };

    constexpr auto estimate(real_t left, real_t right, auto const& integrand) const noexcept -> estimate_t
    {
        auto const width = right - left;
        auto const sum   = width * (integrand(left) + integrand(right)) / static_cast<real_t>(2.0);

        // fake a small internal error; abs() to honor the non-negative contract under reversed bounds
        auto const error = abs(width) * static_cast<real_t>(0.1);

        return {sum, error};
    }

    constexpr auto integrate(real_t left, real_t right, auto const& integrand) const noexcept -> real_t
    {
        return estimate(left, right, integrand).sum;
    }
};

// f(x) = x^2
template <std::floating_point real_t> struct quadratic_integrand_t
{
    constexpr auto operator()(real_t x) const noexcept -> real_t { return x * x; }
};

// f(x) = x^3
template <std::floating_point real_t> struct cubic_integrand_t
{
    constexpr auto operator()(real_t x) const noexcept -> real_t { return x * x * x; }
};

// f(x) = value, stateful
template <std::floating_point real_t> struct constant_integrand_t
{
    real_t         value;
    constexpr auto operator()(real_t) const noexcept -> real_t { return value; }
};

// f(x) = -x^2
template <std::floating_point real_t> struct negative_quadratic_integrand_t
{
    constexpr auto operator()(real_t x) const noexcept -> real_t { return -(x * x); }
};

template <typename integrand_t, typename rule_t> struct integral_t
{
    integrand_t integrand;
    rule_t      rule;

    template <std::floating_point real_t>
    constexpr auto estimate(real_t left, real_t right) const noexcept -> rule_t::estimate_t
    {
        return rule.estimate(left, right, integrand);
    }

    template <std::floating_point real_t> constexpr auto integrate(real_t left, real_t right) const noexcept -> real_t
    {
        return rule.integrate(left, right, integrand);
    }
};

namespace float64_test_t {

using real_t = float64_t;

constexpr auto rule                = rule_t<real_t>{};
constexpr auto quadratic_integrand = quadratic_integrand_t<real_t>{};

// ====================================================================================================================
// even function
// ====================================================================================================================

namespace even_function {

constexpr auto sut
    = bisector_t<integral_t<quadratic_integrand_t<real_t>, rule_t<real_t>>>{integral_t{quadratic_integrand, rule}};

// --------------------------------------------------------------------------------------------------------------------
// root segment creation
// --------------------------------------------------------------------------------------------------------------------

constexpr auto parent = sut(0.0, 6.0, initial_tolerance);

// parent bounds
static_assert(parent.left == 0.0);
static_assert(parent.right == 6.0);
static_assert(parent.tolerance == initial_tolerance);
static_assert(parent.depth == 0);

// trapezoidal of x^2 from 0 to 6: 6 * (0 + 36) / 2 = 108
static_assert(parent.integral == 108.0);

// --------------------------------------------------------------------------------------------------------------------
// root segment bisection
// --------------------------------------------------------------------------------------------------------------------

constexpr auto bisection = sut(parent);

static_assert(bisection.integral == 81.0); // 13.5 + 67.5

// left child bounds
static_assert(bisection.left.left == 0.0);
static_assert(bisection.left.right == 3.0);
static_assert(bisection.left.tolerance == initial_tolerance * 0.5);
static_assert(bisection.left.depth == 1);

// trapezoidal of x^2 from 0 to 3: 3 * (0 + 9) / 2 = 13.5
static_assert(bisection.left.integral == 13.5);

// right child bounds
static_assert(bisection.right.left == 3.0);
static_assert(bisection.right.right == 6.0);
static_assert(bisection.right.tolerance == initial_tolerance * 0.5);
static_assert(bisection.right.depth == 1);

// trapezoidal of x^2 from 3 to 6: 3 * (9 + 36) / 2 = 67.5
static_assert(bisection.right.integral == 67.5);

// error estimate is dominated by subdivision error
// left rule quadrature error: 3.0 * 0.1 = 0.3
// right rule quadrature error: 3.0 * 0.1 = 0.3
// quadrature error sum = 0.6
// subdivision error = abs(81.0 - 108.0) = 27.0
// max error = max(0.6, 27.0) = 27.0
static_assert(bisection.error_estimate == 27.0);

// --------------------------------------------------------------------------------------------------------------------
// nested bisection
// --------------------------------------------------------------------------------------------------------------------

// bisecting a child verifies that depth and tolerance keep propagating past the first level
constexpr auto nested_bisection = sut(bisection.left);

// depth increments off the child, not the root
static_assert(nested_bisection.left.depth == 2);
static_assert(nested_bisection.right.depth == 2);

// tolerance halves again, so 1/4 of the root tolerance
static_assert(nested_bisection.left.tolerance == initial_tolerance * 0.25);
static_assert(nested_bisection.right.tolerance == initial_tolerance * 0.25);

// left child of [0, 3] is [0, 1.5]; right child is [1.5, 3]
static_assert(nested_bisection.left.left == 0.0);
static_assert(nested_bisection.left.right == 1.5);
static_assert(nested_bisection.right.left == 1.5);
static_assert(nested_bisection.right.right == 3.0);

// --------------------------------------------------------------------------------------------------------------------
// zero-width segment
// --------------------------------------------------------------------------------------------------------------------

constexpr auto zero_width_parent = sut(5.0, 5.0, initial_tolerance);
static_assert(zero_width_parent.left == 5.0);
static_assert(zero_width_parent.right == 5.0);
static_assert(zero_width_parent.integral == 0.0);

constexpr auto zero_width_bisection = sut(zero_width_parent);
static_assert(zero_width_bisection.left.integral == 0.0);
static_assert(zero_width_bisection.right.integral == 0.0);
static_assert(zero_width_bisection.integral == 0.0);
static_assert(zero_width_bisection.error_estimate == 0.0);

// --------------------------------------------------------------------------------------------------------------------
// reversed bounds
// --------------------------------------------------------------------------------------------------------------------

constexpr auto reversed_parent = sut(6.0, 0.0, initial_tolerance);

// parent (6.0 to 0.0), width is -6.0, trapezoidal: -6.0 * (36 + 0) / 2 = -108.0
static_assert(reversed_parent.left == 6.0);
static_assert(reversed_parent.right == 0.0);
static_assert(reversed_parent.tolerance == initial_tolerance);
static_assert(reversed_parent.depth == 0);
static_assert(reversed_parent.integral == -108.0);

constexpr auto reversed_bisection = sut(reversed_parent);

// left child (6.0 to 3.0), width is -3.0, trapezoidal: -3.0 * (36 + 9) / 2 = -67.5
static_assert(reversed_bisection.left.left == 6.0);
static_assert(reversed_bisection.left.right == 3.0);
static_assert(reversed_bisection.left.tolerance == initial_tolerance * 0.5);
static_assert(reversed_bisection.left.depth == 1);
static_assert(reversed_bisection.left.integral == -67.5);

// right child (3.0 to 0.0), width is -3.0. trapezoidal: -3.0 * (9 + 0) / 2 = -13.5
static_assert(reversed_bisection.right.left == 3.0);
static_assert(reversed_bisection.right.right == 0.0);
static_assert(reversed_bisection.right.tolerance == initial_tolerance * 0.5);
static_assert(reversed_bisection.right.depth == 1);
static_assert(reversed_bisection.right.integral == -13.5);

// combined integral is negative
static_assert(reversed_bisection.integral == -81.0);

// error estimate is still positive
static_assert(reversed_bisection.error_estimate == 27.0);

} // namespace even_function

// ====================================================================================================================
// odd function, symmetric cancellation
// ====================================================================================================================

namespace odd_function {

using integrand_t        = cubic_integrand_t<real_t>;
constexpr auto integrand = integrand_t{};
constexpr auto sut       = bisector_t<integral_t<integrand_t, rule_t<real_t>>>{integral_t{integrand, rule}};

constexpr auto parent = sut(-2.0, 2.0, initial_tolerance);

// width is 4.0, trapezoidal: 4.0 * (-8 + 8) / 2 = 0.0
static_assert(parent.integral == 0.0);

constexpr auto bisection = sut(parent);

// left child (-2.0 to 0.0), width is 2.0, trapezoidal: 2.0 * (-8 + 0) / 2 = -8.0
static_assert(bisection.left.integral == -8.0);

// right child (0.0 to 2.0), width is 2.0, trapezoidal: 2.0 * (0 + 8) / 2 = 8.0
static_assert(bisection.right.integral == 8.0);

// combined should perfectly cancel back out to 0.0
static_assert(bisection.integral == 0.0);

} // namespace odd_function

// ====================================================================================================================
// constant function
// subdivision error is exactly zero, so quadrature error wins by default
// incidentally exercises stateful integrand storage: constant_integrand_t holds its return value as a member
// ====================================================================================================================

namespace const_function {

using integrand_t        = constant_integrand_t<real_t>;
constexpr auto integrand = integrand_t{10.0};
constexpr auto sut       = bisector_t<integral_t<integrand_t, rule_t<real_t>>>{integral_t{integrand, rule}};

constexpr auto parent = sut(0.0, 4.0, initial_tolerance);

// parent integral: 4.0 * 10.0 = 40.0
static_assert(parent.integral == 40.0);

constexpr auto bisection = sut(parent);

// combined integral matches parent, meaning subdivision_error is 0.0
static_assert(bisection.integral == 40.0);

// error estimate is dominated by quadrature error
// left rule error: 2.0 * 0.1 = 0.2
// right rule error: 2.0 * 0.1 = 0.2
// quadrature error sum = 0.4
// subdivision error = abs(40.0 - 40.0) = 0.0
// max error = max(0.4, 0.0) = 0.4
static_assert(bisection.error_estimate == 0.4);

} // namespace const_function

// ====================================================================================================================
// quadrature error wins with both components positive
// the const_function namespace tests the degenerate case where subdivision_error is 0; this verifies the max() picks
// the larger of two strictly positive values
// ====================================================================================================================

namespace quadrature_dominant {

// reuse the quadratic integrand on a narrow interval, so subdivision error shrinks below quadrature error
constexpr auto sut
    = bisector_t<integral_t<quadratic_integrand_t<real_t>, rule_t<real_t>>>{integral_t{quadratic_integrand, rule}};

constexpr auto parent = sut(0.0, 0.5, initial_tolerance);

// parent: 0.5 * (0 + 0.25) / 2 = 0.0625
static_assert(parent.integral == 0.0625);

constexpr auto bisection = sut(parent);

// left  [0, 0.25]:   0.25 * (0       + 0.0625) / 2 = 0.0078125
// right [0.25, 0.5]: 0.25 * (0.0625  + 0.25)   / 2 = 0.0390625
// sum                                              = 0.046875
static_assert(bisection.integral == 0.046875);

// quadrature error sum: 2 * (0.25 * 0.1)            = 0.05
// subdivision error:    abs(0.046875 - 0.0625)      = 0.015625
// max(0.05, 0.015625) = 0.05
// both positive, quadrature wins
static_assert(bisection.error_estimate == 0.05);

} // namespace quadrature_dominant

// ====================================================================================================================
// negative function, negative area
// ====================================================================================================================

namespace negative_function {

using integrand_t        = negative_quadratic_integrand_t<real_t>;
constexpr auto integrand = integrand_t{};
constexpr auto sut       = bisector_t<integral_t<integrand_t, rule_t<real_t>>>{integral_t{integrand, rule}};

constexpr auto parent = sut(0.0, 6.0, initial_tolerance);

// width is 6.0, trapezoidal: 6.0 * (0 - 36) / 2 = -108.0
static_assert(parent.integral == -108.0);

constexpr auto bisection = sut(parent);

// left  [0, 3]: 3.0 * (0 + -9)   / 2 = -13.5
// right [3, 6]: 3.0 * (-9 + -36) / 2 = -67.5
static_assert(bisection.left.integral == -13.5);
static_assert(bisection.right.integral == -67.5);
static_assert(bisection.integral == -81.0);

// subdivision error = abs(-81.0 - (-108.0)) = 27.0
// max error = max(0.6, 27.0) = 27.0
static_assert(bisection.error_estimate == 27.0);

} // namespace negative_function
} // namespace float64_test_t

// ====================================================================================================================
// float32
// this must compile without narrowing or conversion warnings
// ====================================================================================================================

namespace float32_test {

using real_t        = float32_t;
constexpr auto rule = rule_t<real_t>{};

using integrand_t        = quadratic_integrand_t<real_t>;
constexpr auto integrand = integrand_t{};
constexpr auto sut       = bisector_t<integral_t<integrand_t, rule_t<real_t>>>{integral_t{integrand, rule}};

constexpr auto parent    = sut(0.0f, 6.0f, 1.0f);
constexpr auto bisection = sut(parent);

static_assert(bisection.integral == 81.0f);

} // namespace float32_test

} // namespace
} // namespace crv::quadrature
