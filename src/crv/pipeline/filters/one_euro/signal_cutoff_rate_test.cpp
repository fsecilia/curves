// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "signal_cutoff_rate.hpp"
#include <crv/math/limits.hpp>
#include <crv/test/test.hpp>

namespace crv::pipeline::filters::one_euro {
namespace {

using cutoff_rate_t = fixed_t<int8_t, 4>;
using cutoff_slope_t = fixed_t<int8_t, 4>;
using dx_t = fixed_t<int8_t, 4>;

constexpr auto minimum = cutoff_rate_t{1};

static_assert(signal_cutoff_rate(minimum, cutoff_slope_t{}, dx_t{3}) == minimum);
static_assert(signal_cutoff_rate(minimum, cutoff_slope_t::literal(8), dx_t{2}) == cutoff_rate_t{2});
static_assert(signal_cutoff_rate(minimum, cutoff_slope_t::literal(8), dx_t{-2}) == cutoff_rate_t{2});
static_assert(signal_cutoff_rate(minimum, cutoff_slope_t::literal(1), min<dx_t>()) == cutoff_rate_t::literal(24));

// minimum = 1.0
// adaptive = 1/16 * 1/2 = 1/32, exactly halfway between output quanta.
// raw 16 is even, so nearest-even rounds down.
static_assert(signal_cutoff_rate(cutoff_rate_t::literal(16), cutoff_slope_t::literal(1), dx_t::literal(8))
    == cutoff_rate_t::literal(16));

// Same half-quantum contribution, but raw 17 is odd, so nearest-even rounds up.
static_assert(signal_cutoff_rate(cutoff_rate_t::literal(17), cutoff_slope_t::literal(1), dx_t::literal(8))
    == cutoff_rate_t::literal(18));

constexpr auto maximum_result = try_signal_cutoff_rate(cutoff_rate_t{1}, cutoff_slope_t{1}, dx_t::literal(111));
static_assert(!maximum_result.overflows);
static_assert(maximum_result.value == max<cutoff_rate_t>());

constexpr auto first_overflow = try_signal_cutoff_rate(cutoff_rate_t{1}, cutoff_slope_t{1}, dx_t::literal(112));
static_assert(first_overflow.overflows);

} // namespace
} // namespace crv::pipeline::filters::one_euro
