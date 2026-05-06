// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "approximant.hpp"
#include <crv/math/abs.hpp>
#include <crv/test/test.hpp>
#include <stdexcept>

namespace crv::spline {
namespace {

using real_t = float_t;
using jet_t = jet_t<real_t>;
using x_t = fixed_t<int16_t, 3>;
using y_t = fixed_t<int32_t, 2>;
using normalized_t = int16_t; // normally Q0.64
using coeffs_t = int32_t; // normally an array of signed fixed

constexpr auto log2_width = 4;
constexpr auto x_real = 5; // the initial input
constexpr auto x0 = x_t{2};
constexpr auto x_fixed = x_t::literal(24); // after converting via to_fixed<x_t>(x_real) - x0
constexpr auto y_real = 7;
constexpr auto y = y_t::literal(28); // y_real*2^y_t::frac_bits = 7*2^2 = 28
constexpr auto t = normalized_t{17};
constexpr auto coeffs = coeffs_t{37};
constexpr auto dy_dx = 11.12;

struct segment_t
{
    using x_t = x_t;

    constexpr auto x_to_t(x_t x) const -> normalized_t
    {
        if (x != spline::x_fixed) throw std::runtime_error{"unexpected x"};
        return spline::t;
    }

    constexpr auto evaluate(normalized_t t) const -> y_t
    {
        if (t != spline::t) throw std::runtime_error{"unexpected t"};
        return spline::y;
    }

    constexpr auto log2_width() const noexcept -> int_t { return spline::log2_width; }
    constexpr auto coeffs() const noexcept -> coeffs_t { return spline::coeffs; }
};

struct segment_derivative_t
{
    constexpr auto dy_dx(coeffs_t coeffs, normalized_t t, int_t log2_width) const -> real_t
    {
        if (coeffs != spline::coeffs) throw std::runtime_error{"unexpected coeffs"};
        if (t != spline::t) throw std::runtime_error{"unexpected t"};
        if (log2_width != spline::log2_width) throw std::runtime_error{"unexpected log2_width"};
        return spline::dy_dx;
    }
};

using sut_t = approximant_t<real_t, segment_t, segment_derivative_t>;

// execution logic
TEST(spline_approximant_test, test)
{
    auto const sut = sut_t{.x0 = x0, .segment = segment_t{}, .segment_derivative = segment_derivative_t{}};
    auto const expected = jet_t{y_real, dy_dx};
    auto const actual = sut(x_real);
    EXPECT_EQ(expected, actual);
}

} // namespace
} // namespace crv::spline
