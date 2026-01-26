// SPDX-License-Identifier: MIT
/**
  \file
  \brief fixed point integer type

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <linux/types.h>

namespace crv {

template <typename value_type, int t_frac_bits> struct fixed_t;
using fixed_q15_0_t  = fixed_t<s16, 0>;
using fixed_q32_32_t = fixed_t<s64, 32>;
using fixed_q0_64_t  = fixed_t<u64, 64>;

/// fixed-point arithmetic type with statically-configurable precision
template <typename value_type, int t_frac_bits> struct fixed_t
{
    using value_t = value_type;
    value_t value;

    static constexpr auto frac_bits = t_frac_bits;

    /// default initializer - value is unspecified
    constexpr fixed_t() = default;

    /// value initializer - value is specified directly; it is not rescaled to frac_bits
    constexpr fixed_t(value_t value) noexcept : value{value} {}

    constexpr auto operator==(fixed_t const&) const noexcept -> bool  = default;
    constexpr auto operator<=>(fixed_t const&) const noexcept -> auto = default;
};

} // namespace crv
