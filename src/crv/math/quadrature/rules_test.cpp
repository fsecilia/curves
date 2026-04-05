// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "rules.hpp"
#include <crv/math/abs.hpp>
#include <crv/test/test.hpp>

namespace crv::quadrature::rules {
namespace {

using real_t = float_t;

constexpr auto near(real_t a, real_t b, real_t tolerance = 1e-12) noexcept -> bool
{
    return abs(a - b) <= tolerance;
}

constexpr auto rule = gauss5_t{};

// Degree 2 polynomial: f(x) = x^2
// Integral from 0 to 2 is x^3 / 3 = 8 / 3.
static_assert(near(rule(0.0, 2.0, [](real_t x) { return x * x; }), 8.0 / 3.0));

// Degree 8 polynomial: f(x) = x^8
// Integral from -1 to 1 is x^9 / 9 = 2 / 9.
static_assert(near(rule(-1.0, 1.0,
                        [](double x) {
                            auto const x2 = x * x;
                            auto const x4 = x2 * x2;
                            return x4 * x4;
                        }),
                   2.0 / 9.0));

} // namespace
} // namespace crv::quadrature::rules
