// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/abs.hpp>
#include <cmath>
#include <concepts>
#include <limits>
#include <numbers>

namespace crv {

// calcs the principal branch of the Lambert W function, W_0(x), using Halley's method with a fixed number of iterations
template <std::floating_point value_t, int_t num_iterations = 20>
constexpr auto lambert_w0(value_t x) noexcept -> value_t
{
    using crv::abs;
    using std::exp;
    using std::log;

    constexpr auto negative_reciprocal_e = -value_t{1} / std::numbers::e_v<value_t>;

    // check domain for real results on principal branch
    if (x < negative_reciprocal_e) return std::numeric_limits<value_t>::quiet_NaN();
    if (x == 0.0) return 0.0;

    // initial guess: x for small values, ln(x) for large values
    auto w = (x < value_t{1.0}) ? x : log(x);

    // Halley's method loop
    //
    // Halley's method is Newton-Raphson with an extra derivative. It converges cubically rather than quadratically.
    for (auto i = int_t{0}; i < num_iterations; ++i)
    {
        auto const ew = exp(w);
        auto const wew = w * ew;
        auto const w1 = w + value_t{1.0};
        auto const difference = wew - x;

        // check tolerance relative to x scale
        if (abs(difference) <= std::numeric_limits<value_t>::epsilon() * abs(x)) break;

        // Halley step: w_{n+1} = w_n - f(w)/(f'(w) - (f''(w) * f(w)/(2 f'(w))))
        auto const step = difference / (ew * w1 - ((w + value_t{2.0}) / (value_t{2.0} * w1)) * difference);
        w -= step;
    }

    return w;
}

} // namespace crv
