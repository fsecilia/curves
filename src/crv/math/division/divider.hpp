// SPDX-License-Identifier: MIT

/// \file
/// \brief division stack
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/algorithm.hpp>
#include <crv/math/division/hardware_divider.hpp>
#include <crv/math/division/shifted_int_divider.hpp>
#include <crv/math/division/wide_divider.hpp>
#include <crv/math/int_traits.hpp>

namespace crv::division {

namespace detail {

// choose the smallest native word whose wide form contains the dividend and whose narrow form contains the divisor
template <integral lhs_t, integral rhs_t>
using native_division_word_t = int_by_bytes_t<max(sizeof(rhs_t), (sizeof(lhs_t) + 1) / 2), false>;

// fully-compose the native, wide, and scaled division machinery
template <integral out_value_t, integral lhs_t, integral rhs_t, int_t shift, bool saturate>
using divider_t = shifted_int_divider_t<wide_divider_t<native_division_word_t<lhs_t, rhs_t>,
                                            hardware_divider_t<native_division_word_t<lhs_t, rhs_t>>>,
    shift, out_value_t, lhs_t, rhs_t, saturate>;

} // namespace detail

/// rounded and scaled integer division operation
template <integral out_value_t, integral lhs_t, integral rhs_t, int_t shift, bool saturate = true>
inline constexpr auto divide = detail::divider_t<out_value_t, lhs_t, rhs_t, shift, saturate>{};

} // namespace crv::division
