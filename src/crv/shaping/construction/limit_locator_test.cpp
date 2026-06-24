// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "limit_locator.hpp"
#include <crv/test/test.hpp>

namespace crv::shaping::construction {
namespace {

using real_t = float_t;

// y = 2x
struct curve_t
{
    constexpr auto operator()(real_t x) const noexcept -> real_t { return x * 2.0; }
};
static_assert(is_curve<curve_t, real_t>);

// inverter for curve_t, y = 2x
struct inverter_t
{
    constexpr auto operator()(real_t min, real_t max, real_t limit, auto const& /*curve*/) const noexcept
        -> std::optional<real_t>
    {
        auto const x = limit / 2.0;
        if (min <= x && x <= max) { return x; }
        return std::nullopt;
    }
};
static_assert(is_inverter<inverter_t, real_t, curve_t>);

constexpr auto curve = curve_t{};

using sut_t = limit_locator_t<real_t, inverter_t>;
constexpr auto sut = sut_t{inverter_t{}};

// limit reached within bounds, zero transition width
static_assert(5.0 == sut(0.0, 10.0, 0.0, 0.0, 10.0, curve));

// limit reached within bounds, nonzero transition width
static_assert(4.0 == sut(0.0, 10.0, 0.5, 2.0, 10.0, curve));

// limit not reached within bounds, falls back to input_max, zero transition width
static_assert(10.0 == sut(0.0, 10.0, 0.0, 0.0, 30.0, curve));

// limit not reached within bounds, falls back to input_max, nonzero transition width
static_assert(9.0 == sut(0.0, 10.0, 0.5, 2.0, 30.0, curve));

// limit reached exactly at input_min
static_assert(-1.0 == sut(0.0, 10.0, 0.5, 2.0, 0.0, curve));

// limit reached exactly at input_max
static_assert(9.0 == sut(0.0, 10.0, 0.5, 2.0, 20.0, curve));

// domain entirely in negative space, limit reached within bounds
static_assert(-8.0 == sut(-10.0, -5.0, 0.5, 2.0, -14.0, curve));

// domain crossing origin, limit reached within bounds
static_assert(-1.0 == sut(-5.0, 5.0, 0.5, 2.0, 0.0, curve));

// zero transition width, non-zero transition max
static_assert(5.0 == sut(0.0, 10.0, 0.0, 5.0, 10.0, curve));

// non-zero transition width, zero transition max
static_assert(5.0 == sut(0.0, 10.0, 2.0, 0.0, 10.0, curve));

// limit below curve's range (invert returns nullopt because -2.0 / 2.0 < 0.0).
static_assert(9.0 == sut(0.0, 10.0, 0.5, 2.0, -2.0, curve));

} // namespace
} // namespace crv::shaping::construction
