// SPDX-License-Identifier: MIT
/**
    \file
    \brief fixed point integer type

    \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/integer.hpp>
#include <crv/math/limits.hpp>
#include <algorithm>
#include <concepts>

namespace crv {

template <integral value_type, int_t t_frac_bits> struct fixed_t;
using fixed_q15_0_t  = fixed_t<int16_t, 0>;
using fixed_q32_32_t = fixed_t<int64_t, 32>;
using fixed_q0_64_t  = fixed_t<uint64_t, 64>;

/// fixed-point arithmetic type with statically-configurable precision
template <integral value_type, int_t t_frac_bits> struct fixed_t
{
    using value_t                   = value_type;
    static constexpr auto frac_bits = t_frac_bits;

    value_t value;

    // ----------------------------------------------------------------------------------------------------------------
    // Construction
    // ----------------------------------------------------------------------------------------------------------------

    /// default initializer matches underlying
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

    // ----------------------------------------------------------------------------------------------------------------
    // Binary Arithmetic
    // ----------------------------------------------------------------------------------------------------------------

    constexpr auto operator+=(fixed_t const& src) noexcept -> fixed_t&
    {
        value += src.value;
        return *this;
    }

    constexpr auto operator-=(fixed_t const& src) noexcept -> fixed_t&
    {
        value -= src.value;
        return *this;
    }

    constexpr auto operator*=(fixed_t const& src) noexcept -> fixed_t& { return *this = fixed_t{*this * src}; }

    friend constexpr auto operator+(fixed_t lhs, fixed_t const& rhs) noexcept -> fixed_t { return lhs += rhs; }
    friend constexpr auto operator-(fixed_t lhs, fixed_t const& rhs) noexcept -> fixed_t { return lhs -= rhs; }

    // ----------------------------------------------------------------------------------------------------------------
    // Math Functions
    // ----------------------------------------------------------------------------------------------------------------

    friend constexpr auto abs(fixed_t const& src) noexcept -> fixed_t
        requires(std::unsigned_integral<value_t>)
    {
        return src;
    }

    friend constexpr auto abs(fixed_t const& src) noexcept -> fixed_t
        requires(std::signed_integral<value_t>)
    {
        return src.value < 0 ? fixed_t{-src.value} : src;
    }

private:
    template <integral other_value_t, int_t other_frac_bits>
    static constexpr auto convert_value(other_value_t const& other_value) noexcept -> value_t
    {
        static constexpr auto max_type_size = std::max(sizeof(value_t), sizeof(other_value_t));
        using wider_t                       = sized_integer_t<max_type_size, signed_integral<other_value_t>>;

        if constexpr (frac_bits > other_frac_bits)
        {
            // shift left using wider type
            return static_cast<value_t>(static_cast<wider_t>(other_value) << (frac_bits - other_frac_bits));
        }
        else if constexpr (other_frac_bits > frac_bits)
        {
            constexpr auto shift = other_frac_bits - frac_bits;
            auto const     wider = static_cast<wider_t>(other_value);
            return static_cast<value_t>((wider >> shift) + ((wider >> (shift - 1)) & static_cast<wider_t>(1)));
        }
        else
        {
            return static_cast<value_t>(other_value);
        }
    }
};

namespace detail::fixed {

template <typename lhs_value_t, int_t lhs_frac_bits, typename rhs_value_t, int_t rhs_frac_bits>
struct multiplication_result_t
{
    static constexpr auto larger_type_size   = 2 * std::max(sizeof(lhs_value_t), sizeof(rhs_value_t));
    static constexpr auto either_type_signed = is_signed_v<lhs_value_t> || is_signed_v<rhs_value_t>;
    static constexpr auto result_frac_bits   = lhs_frac_bits + rhs_frac_bits;

    using value_t = sized_integer_t<larger_type_size, either_type_signed>;
    using type    = fixed_t<value_t, result_frac_bits>;
};

} // namespace detail::fixed

template <typename lhs_value_t, int_t lhs_frac_bits, typename rhs_value_t, int_t rhs_frac_bits>
using multiplication_result_t
    = detail::fixed::multiplication_result_t<lhs_value_t, lhs_frac_bits, rhs_value_t, rhs_frac_bits>::type;

template <typename lhs_value_t, int_t lhs_frac_bits, typename rhs_value_t, int_t rhs_frac_bits>
constexpr auto operator*(fixed_t<lhs_value_t, lhs_frac_bits>        lhs,
                         fixed_t<rhs_value_t, rhs_frac_bits> const& rhs) noexcept
    -> multiplication_result_t<lhs_value_t, lhs_frac_bits, rhs_value_t, rhs_frac_bits>

{
    using result_t = multiplication_result_t<lhs_value_t, lhs_frac_bits, rhs_value_t, rhs_frac_bits>;

    // The outer cast here is necessary because if the value type is smaller than int, it is promoted.
    auto const product = static_cast<result_t::value_t>(static_cast<result_t::value_t>(lhs.value) * rhs.value);

    return result_t{product};
}

/**
    division, stripped down for pipeline

    This is not a general implementation. The pipeline has one big divide, and this implementation considers its
    constraints:
        - all values are unsigned
        - all values are 64-bit
        - the output precision is at least as high as the dividend
    This simplifies the implementation compared to a general fixed/fixed with mixed signs and sizes.
*/
template <int_t out_frac_bits, int_t lhs_frac_bits, int_t rhs_frac_bits>
auto divide(fixed_t<uint64_t, lhs_frac_bits> const& lhs, fixed_t<uint64_t, rhs_frac_bits> const& rhs) noexcept
    -> fixed_t<uint64_t, out_frac_bits>
{
    constexpr auto total_shift = rhs_frac_bits + out_frac_bits - lhs_frac_bits;

    auto const divisor         = rhs.value;
    auto const divides_by_zero = 0 == divisor;
    if (divides_by_zero) [[unlikely]] { return {max<uint64_t>()}; }

    auto const dividend           = static_cast<uint128_t>(lhs.value) << total_shift;
    auto const quotient_overflows = (dividend >> 64) >= divisor;
    if (quotient_overflows) [[unlikely]] { return {max<uint64_t>()}; }

    auto [quotient, remainder] = div_u128_u64(dividend, divisor);

    quotient += remainder >= (divisor - remainder);

    return {quotient};
}

} // namespace crv
