// SPDX-License-Identifier: MIT

/// \file
/// \brief division-specific concepts
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/division/qr_pair.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/rounding_mode.hpp>
#include <type_traits>

namespace crv::division {

/// takes unsigned wide dividend, unsigned narrow divisor, returns {quotient, remainder} pair
template <typename divider_t, typename narrow_t>
concept is_hardware_divider
    = unsigned_integral<narrow_t> && requires(divider_t const& divider, wider_t<narrow_t> dividend, narrow_t divisor) {
          { divider(dividend, divisor) } -> std::same_as<qr_pair_t<narrow_t>>;
      };

/// takes unsigned wide dividend, unsigned narrow divisor, and rounding mode; returns wide quotient
template <typename divider_t, typename narrow_t, typename rounding_mode_t>
concept is_wide_divider
    = unsigned_integral<narrow_t> && is_div_rounding_mode<rounding_mode_t, wider_t<narrow_t>, narrow_t>
      && requires(divider_t const& divider, wider_t<narrow_t> dividend, narrow_t divisor,
                  rounding_mode_t rounding_mode) {
             { divider(dividend, divisor, rounding_mode) } -> std::same_as<wider_t<narrow_t>>;
         };

/// takes unsigned narrow dividend, unsigned divisor, and rounding mode; returns wide quotient
template <typename divider_t, typename narrow_t, typename rounding_mode_t>
concept is_narrow_divider
    = unsigned_integral<narrow_t> && is_div_rounding_mode<rounding_mode_t, wider_t<narrow_t>, narrow_t>
      && requires(divider_t const& divider, narrow_t dividend, narrow_t divisor, rounding_mode_t rounding_mode) {
             { divider(dividend, divisor, rounding_mode) } -> std::same_as<wider_t<narrow_t>>;
         };

} // namespace crv::division
