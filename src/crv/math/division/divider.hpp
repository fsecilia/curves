// SPDX-License-Identifier: MIT

/// \file
/// \brief division stack
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/division/hardware_divider.hpp>
#include <crv/math/division/shifted_int_divider.hpp>
#include <crv/math/division/wide_divider.hpp>

namespace crv::division {

/// fully-composed division stack
template <unsigned_integral narrow_t, int total_shift, bool saturates = true>
using divider_t = shifted_int_divider_t<wide_divider_t<narrow_t, hardware_divider_t<narrow_t>>, total_shift, saturates>;

/// convenience variable template
template <unsigned_integral narrow_t, int total_shift, bool saturates = true>
inline constexpr auto divider = divider_t<narrow_t, total_shift, saturates>{};

} // namespace crv::division
