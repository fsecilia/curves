// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "pipeline_config.hpp"
#include <crv/lib.hpp>
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

// x_t = fixed_t<int64_t, 45>
//
// Choose a generous human physical limit of 1000 in/s. Saturating at 128kdpi gives a max rate of 128k counts/ms:
//
//     1000 in/s * 1 s/1000 ms * 128000 counts/in = 128000 counts/ms
//     sqrt(2*128000^2) = sqrt(2)*128000 ~= 181019.335983756
//     log2(181019.335983756) ~= 17.465784285 bits
//
// This requires 18 integer bits, which gives Q18.45.
static_assert(prod_pipeline_config_t::x_t::frac_bits == 45);

// y_t = fixed_t<int64_t, 45>
//
// For a domain of [0, 2^8) and soft limiter on the integrand at y=1000, the largest integral possible is a pinned
// straight line, integrating to 256000. The integer limit of Q18.45 is 262144, giving 6,144/256,000 = 0.024 = 2.4%
// headroom.
static_assert(prod_pipeline_config_t::y_t::frac_bits == 45);

// shift widths must be large enough to hold 7 bit shifts
static_assert(prod_pipeline_config.segment_layout.intermediate.shift_width == 7);
static_assert(prod_pipeline_config.segment_layout.final.shift_width == 7);

// intermediate shifts are strictly right, but the final shift can be in either direction
static_assert(!prod_pipeline_config.segment_layout.intermediate.is_signed);
static_assert(prod_pipeline_config.segment_layout.final.is_signed);

} // namespace
} // namespace crv::spline
