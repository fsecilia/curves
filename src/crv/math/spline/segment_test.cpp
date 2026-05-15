// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "segment.hpp"
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

// --------------------------------------------------------------------------------------------------------------------
// layouts
// --------------------------------------------------------------------------------------------------------------------

static_assert(intermediate_layout.shift_mask() == 0x3F); // 6 bits
static_assert(final_layout.shift_mask() == 0x7F); // 7 bits

} // namespace
} // namespace crv::spline
