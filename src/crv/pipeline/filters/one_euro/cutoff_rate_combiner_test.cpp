// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "cutoff_rate_combiner.hpp"
#include <crv/test/test.hpp>

namespace crv::pipeline::filters::one_euro {
namespace {

using cutoff_step_value_t = uint64_t;
using cutoff_step_t = fixed_t<cutoff_step_value_t, 58>;

using magnitude_value_t = uint32_t;
using magnitude_t = fixed_t<magnitude_value_t, 16>;

constexpr auto sut = cutoff_rate_combiner_t<cutoff_step_t>{};
constexpr auto magnitude_one = magnitude_t::literal(magnitude_value_t{1} << 16);
constexpr auto magnitude_half = magnitude_t::literal(magnitude_value_t{1} << 15);
constexpr auto magnitude_quarter = magnitude_t::literal(magnitude_value_t{1} << 14);

constexpr auto adaptive_raw(cutoff_step_value_t raw) noexcept
{
    return multiply(cutoff_step_t::literal(raw), magnitude_one);
}

constexpr auto adaptive_whole(cutoff_step_value_t whole) noexcept
{
    return adaptive_raw(whole << cutoff_step_t::frac_bits);
}

// exact arithmetic below saturation
static_assert(sut(cutoff_step_t{5}, adaptive_whole(10)) == cutoff_step_t{15});

// zero adaptive term passes omega_min through exactly
static_assert(sut(cutoff_step_t::literal(12345), adaptive_raw(0)) == cutoff_step_t::literal(12345));

// bva for the saturating final conversion
constexpr auto near_max = cutoff_step_t::literal(max<cutoff_step_value_t>() - 5);
static_assert(sut(near_max, adaptive_raw(4)) == cutoff_step_t::literal(max<cutoff_step_value_t>() - 1));
static_assert(sut(near_max, adaptive_raw(5)) == max<cutoff_step_t>());
static_assert(sut(near_max, adaptive_raw(6)) == max<cutoff_step_t>());

//
// rne rounding mode checks
//

// 0.5 rounds down
static_assert(
    sut(cutoff_step_t{}, multiply(cutoff_step_t::literal(2), magnitude_quarter)) == cutoff_step_t::literal(0));

// 0.75 still rounds up
static_assert(
    sut(cutoff_step_t{}, multiply(cutoff_step_t::literal(3), magnitude_quarter)) == cutoff_step_t::literal(1));

// 1.5 rounds up
static_assert(
    sut(cutoff_step_t{}, multiply(cutoff_step_t::literal(6), magnitude_quarter)) == cutoff_step_t::literal(2));

//
// order of operations
//
// omega_min and the adaptive term are summed at the adaptive product's precision, then narrowed once.
//
//     omega_min = 1 raw ulp
//     adaptive  = 0.5 raw ulp
//     sum       = 1.5 raw ulps
//     RNE       = 2 raw ulps
//
// Narrowing the adaptive term before addition would produce 0 + 1 = 1.
constexpr auto adaptive_half_ulp = multiply(cutoff_step_t::literal(1), magnitude_half);
static_assert(sut(cutoff_step_t::literal(1), adaptive_half_ulp) == cutoff_step_t::literal(2));

} // namespace
} // namespace crv::pipeline::filters::one_euro
