// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "pipeline_config.hpp"
#include <crv/lib.hpp>
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

// normalized speed
//
// x_t stores 1000-DPI-equivalent counts/ms. At a generous human physical limit of 1000 in/s, normalization gives 1000
// counts/ms per axis:
//
//     1000 in/s * 1000 counts/in / 1000 ms/s = 1000 counts/ms
//     sqrt(2) * 1000 ~= 1414.214 counts/ms
//
// Signed Q11.52 reaches almost 2048 counts/ms, leaving about 45% headroom above the diagonal limit.
static_assert(prod_pipeline_config_t::x_t::int_bits == 11);
static_assert(prod_pipeline_config_t::x_t::frac_bits == 52);

// induced gain
//
// y_t stores induced gain. Runtime gain is strictly limited to 1000. Signed Q10.53 reaches almost 1024, leaving about
// 2.4% headroom and using the remaining payload bits for gain precision.
static_assert(prod_pipeline_config_t::y_t::int_bits == 10);
static_assert(prod_pipeline_config_t::y_t::frac_bits == 53);

static_assert(!std::same_as<prod_pipeline_config_t::x_t, prod_pipeline_config_t::y_t>);

// shift fields hold seven-bit shifts
static_assert(prod_pipeline_config.segment_layout.intermediate.shift_width == 7);
static_assert(prod_pipeline_config.segment_layout.final.shift_width == 7);

// intermediate shifts right; final shift is signed
static_assert(!prod_pipeline_config.segment_layout.intermediate.is_signed);
static_assert(prod_pipeline_config.segment_layout.final.is_signed);

} // namespace
} // namespace crv::spline
