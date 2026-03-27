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
template <std::integral narrow_t, int shift, bool saturate = true>
using divider_t
    = shifted_int_divider_t<wide_divider_t<make_unsigned_t<narrow_t>, hardware_divider_t<make_unsigned_t<narrow_t>>>,
                            shift, saturate>;

/// convenience variable template
template <std::integral narrow_t, int shift, bool saturate = true>
inline constexpr auto divide = divider_t<narrow_t, shift, saturate>{};

} // namespace crv::division
