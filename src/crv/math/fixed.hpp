// SPDX-License-Identifier: MIT
/**
  \file
  \brief fixed point integer type

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>
#include <type_traits>

namespace crv {

template <typename value_type, int_t t_frac_bits> struct fixed_t;
using fixed_q15_0_t  = fixed_t<int16_t, 0>;
using fixed_q32_32_t = fixed_t<int64_t, 32>;
using fixed_q0_64_t  = fixed_t<uint64_t, 64>;

/// fixed-point arithmetic type with statically-configurable precision
template <typename value_type, int_t t_frac_bits> struct fixed_t
{
    using value_t                   = value_type;
    static constexpr auto frac_bits = t_frac_bits;

    value_t value;

    // ----------------------------------------------------------------------------------------------------------------
    // Construction
    // ----------------------------------------------------------------------------------------------------------------

    /// default initializer - value is unspecified
    constexpr fixed_t() = default;

    /// value initializer - value is specified directly; it is not rescaled to frac_bits
    constexpr fixed_t(value_t value) noexcept : value{value} {}

    // ----------------------------------------------------------------------------------------------------------------
    // Copying
    // ----------------------------------------------------------------------------------------------------------------

    constexpr fixed_t(fixed_t const& src) noexcept                      = default;
    constexpr auto operator=(fixed_t const& other) noexcept -> fixed_t& = default;

    // ----------------------------------------------------------------------------------------------------------------
    // Conversions
    // ----------------------------------------------------------------------------------------------------------------

    template <typename other_value_t, int_t other_frac_bits>
    explicit constexpr fixed_t(fixed_t<other_value_t, other_frac_bits> const& other) noexcept
        : fixed_t{convert_value<other_value_t, other_frac_bits>(other.value)}
    {}

    explicit constexpr operator bool() const noexcept { return !!value; }

    // ----------------------------------------------------------------------------------------------------------------
    // Comparison
    // ----------------------------------------------------------------------------------------------------------------

    constexpr auto operator==(fixed_t const&) const noexcept -> bool  = default;
    constexpr auto operator<=>(fixed_t const&) const noexcept -> auto = default;

    // ----------------------------------------------------------------------------------------------------------------
    // Unary Arithmetic
    // ----------------------------------------------------------------------------------------------------------------

    friend constexpr auto operator+(fixed_t const& src) noexcept -> fixed_t { return src; }
    friend constexpr auto operator-(fixed_t const& src) noexcept -> fixed_t { return fixed_t{-src.value}; }

private:
    template <typename other_value_t, int_t other_frac_bits>
    static constexpr auto convert_value(other_value_t const& other_value) noexcept -> value_t
    {
        using wider_t = std::conditional_t<sizeof(value_t) < sizeof(other_value_t), other_value_t, value_t>;
        if constexpr (frac_bits > other_frac_bits)
        {
            return static_cast<value_t>(static_cast<wider_t>(other_value) << (frac_bits - other_frac_bits));
        }
        else if constexpr (other_frac_bits > frac_bits)
        {
            return static_cast<value_t>(static_cast<wider_t>(other_value) >> (other_frac_bits - frac_bits));
        }
        else
        {
            return static_cast<value_t>(other_value);
        }
    }
};

} // namespace crv
