// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "pipeline_config.hpp"
#include <crv/lib.hpp>
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

// x_t = fixed_t<int64_t, 42>
//
// A 32khz mouse fully saturating input at max rate produces a velocity of sqrt(2*(2^15 - 1)^2)*32 ~= 20.5 bits, so
// we need 21 integer bits, which gives Q21.42.
static_assert(prod_pipeline_config_t::x_t::frac_bits == 42);

// y_t = fixed_t<int64_t, 45>
//
// For a domain of [0, 2^8) and soft limiter on the integrand at y=1000, the largest integral possible is a pinned
// straight line, integrating to 256000. The integer limit of Q18.45 is 262144, giving about 0.6% headroom.
static_assert(prod_pipeline_config_t::y_t::frac_bits == 45);

// shift widths must be large enough to hold 7 bit shifts
static_assert(prod_pipeline_config.segment_layout.intermediate.shift_width == 7);
static_assert(prod_pipeline_config.segment_layout.final.shift_width == 7);

// intermediate shifts are strictly right, but the final shift can be in either direction
static_assert(!prod_pipeline_config.segment_layout.intermediate.is_signed);
static_assert(prod_pipeline_config.segment_layout.final.is_signed);

} // namespace
} // namespace crv::spline
