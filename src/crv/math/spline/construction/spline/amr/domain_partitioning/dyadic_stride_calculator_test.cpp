// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "dyadic_stride_calculator.hpp"
#include <crv/test/test.hpp>

namespace crv::spline::generic {
namespace {

using fixed_t = fixed_t<int_t, 8>;
constexpr auto sut = dyadic_stride_calculator_t<fixed_t>{};

constexpr auto fixed(typename fixed_t::value_t value) noexcept -> fixed_t
{
    return fixed_t::literal(value);
}

// stepping from zero is bounded only by largest power of two that fits in distance
static_assert(sut(fixed(0), fixed(7)).value == 4);
static_assert(sut(fixed(0), fixed(8)).value == 8);

// alignment of current position restricts step size (stepping up segment tree)
static_assert(sut(fixed(2), fixed(8)).value == 2);
static_assert(sut(fixed(4), fixed(20)).value == 4);
static_assert(sut(fixed(24), fixed(64)).value == 8);

// distance to target restricts step size (stepping down segment tree)
static_assert(sut(fixed(8), fixed(11)).value == 2);
static_assert(sut(fixed(12), fixed(13)).value == 1);

// exact power-of-two fit where both alignment and distance are matched
static_assert(sut(fixed(8), fixed(16)).value == 8);
static_assert(sut(fixed(16), fixed(32)).value == 16);

// odd current positions strictly restrict step size to minimum grid unit (1)
static_assert(sut(fixed(3), fixed(8)).value == 1);
static_assert(sut(fixed(15), fixed(16)).value == 1);

} // namespace
} // namespace crv::spline::generic
