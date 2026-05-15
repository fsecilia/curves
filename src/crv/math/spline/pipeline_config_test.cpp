// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/lib.hpp>
#include <crv/math/spline/pipeline_config.hpp>
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

static_assert(prod_pipeline_config.segment_layout.intermediate.shift_mask() == 0x3f); // 6 bits
static_assert(prod_pipeline_config.segment_layout.final.shift_mask() == 0x7f); // 7 bits

} // namespace
} // namespace crv::spline
