// SPDX-License-Identifier: MIT
/**
    \file
    \brief fixed point integer type

    The seemingly superfluous casts back to the value type strewn about this module are required because of c promotion
    rules. Broadly, types smaller than int are cast to int when performing arithmetic. It's more nuanced than that, but
    anywhere a promotion happens, we have to cast back.

    \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/integer.hpp>
#include <crv/math/limits.hpp>
#include <crv/math/rounding_modes.hpp>
#include <algorithm>
#include <type_traits>

namespace crv {

template <integral value_t, int frac_bits> struct fixed_t;
using fixed_q15_0_t  = fixed_t<int16_t, 0>;
using fixed_q32_32_t = fixed_t<int64_t, 32>;
using fixed_q0_64_t  = fixed_t<uint64_t, 64>;

// --------------------------------------------------------------------------------------------------------------------
// Concepts
// --------------------------------------------------------------------------------------------------------------------

template <typename type_t> struct is_fixed_f : std::false_type
{};

template <integral value_t, int frac_bits> struct is_fixed_f<fixed_t<value_t, frac_bits>> : std::true_type
{};

template <typename type_t> static constexpr auto is_fixed_v = is_fixed_f<type_t>::value;

template <typename type_t>
concept is_fixed = is_fixed_v<type_t>;

// --------------------------------------------------------------------------------------------------------------------
// Type Traits
// --------------------------------------------------------------------------------------------------------------------

namespace fixed {

template <is_fixed lhs_t, is_fixed rhs_t> struct promotion_traits_t
{
    using lhs_value_t = lhs_t::value_t;
    using rhs_value_t = rhs_t::value_t;

    static constexpr auto either_type_signed = is_signed_v<lhs_value_t> || is_signed_v<rhs_value_t>;
    static constexpr auto value_type_size    = std::max(sizeof(lhs_value_t), sizeof(rhs_value_t));
    static constexpr auto frac_bits          = std::max(lhs_t::frac_bits, rhs_t::frac_bits);

    using promoted_t = fixed_t<sized_integer_t<value_type_size, either_type_signed>, frac_bits>;
};
template <is_fixed lhs_t, is_fixed rhs_t> using promoted_t = promotion_traits_t<lhs_t, rhs_t>::promoted_t;

template <is_fixed lhs_t, is_fixed rhs_t> struct wider_traits_t
{
    using promoted_value_t = promoted_t<lhs_t, rhs_t>::value_t;

    static constexpr auto either_type_signed = is_signed_v<promoted_value_t>;
    static constexpr auto value_type_size    = sizeof(promoted_value_t) * 2;
    static constexpr auto frac_bits          = lhs_t::frac_bits + rhs_t::frac_bits;

    using wider_t = fixed_t<sized_integer_t<value_type_size, either_type_signed>, frac_bits>;
};
template <is_fixed lhs_t, is_fixed rhs_t> using wider_t = wider_traits_t<lhs_t, rhs_t>::wider_t;

/*
    This is asymmetric because that's what fixed_t originally did manually. It will eventually change to truncate to
    match c++ integer rules.
*/
inline constexpr auto default_rounding_mode = rounding_modes::asymmetric;
using default_rounding_mode_t               = std::remove_cv_t<decltype(default_rounding_mode)>;

} // namespace fixed

// --------------------------------------------------------------------------------------------------------------------

/// fixed-point arithmetic type with statically-configurable precision
template <integral value_type, int t_frac_bits> struct fixed_t
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

    template <is_fixed other_t, typename rounding_mode_t = fixed::default_rounding_mode_t>
    explicit constexpr fixed_t(other_t other, rounding_mode_t rounding_mode = {}) noexcept
        : fixed_t{convert(other, rounding_mode)}
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

    friend constexpr auto operator+(fixed_t src) noexcept -> fixed_t { return src; }
    friend constexpr auto operator-(fixed_t src) noexcept -> fixed_t { return fixed_t{-src.value}; }

    // ----------------------------------------------------------------------------------------------------------------
    // Binary Arithmetic
    // ----------------------------------------------------------------------------------------------------------------

    constexpr auto operator+=(fixed_t src) noexcept -> fixed_t&
    {
        value += src.value;
        return *this;
    }

    constexpr auto operator-=(fixed_t src) noexcept -> fixed_t&
    {
        value -= src.value;
        return *this;
    }

    constexpr auto operator*=(fixed_t src) noexcept -> fixed_t&
    {
        return *this = multiply(*this, src, fixed::default_rounding_mode);
    }

    friend constexpr auto operator+(fixed_t lhs, fixed_t rhs) noexcept -> fixed_t { return lhs += rhs; }
    friend constexpr auto operator-(fixed_t lhs, fixed_t rhs) noexcept -> fixed_t { return lhs -= rhs; }
    friend constexpr auto operator*(fixed_t lhs, fixed_t rhs) noexcept -> fixed_t { return lhs *= rhs; }

    template <is_fixed other_t> friend constexpr auto operator*(fixed_t lhs, other_t rhs) noexcept -> fixed_t
    {
        return multiply(lhs, rhs, fixed::default_rounding_mode);
    }

    /// \returns wide product at higher precision
    template <is_fixed rhs_t>
    friend constexpr auto multiply(fixed_t lhs, rhs_t rhs) noexcept -> fixed::wider_t<fixed_t, rhs_t>
    {
        using result_t      = fixed::wider_t<fixed_t, rhs_t>;
        using wider_value_t = result_t::value_t;

        auto const product = int_cast<wider_value_t>(int_cast<wider_value_t>(lhs.value) * rhs.value);

        return result_t{product};
    }

    /// \returns product, widened or narrowed to output type and rescaled to output precision using given rounding mode
    template <is_fixed out_t, is_fixed rhs_t, typename rounding_mode_t>
    friend constexpr auto multiply(fixed_t lhs, rhs_t rhs, rounding_mode_t rounding_mode) noexcept -> out_t
    {
        return out_t{multiply(lhs, rhs), rounding_mode};
    }

    /// \returns product, narrowed to lhs type and rescaled to lhs precision using given rounding mode
    template <is_fixed rhs_t, typename rounding_mode_t>
    friend constexpr auto multiply(fixed_t lhs, rhs_t rhs, rounding_mode_t rounding_mode) noexcept -> fixed_t
    {
        return multiply<fixed_t, rhs_t, rounding_mode_t>(lhs, rhs, rounding_mode);
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Math Functions
    // ----------------------------------------------------------------------------------------------------------------

    friend constexpr auto abs(fixed_t src) noexcept -> fixed_t
        requires(std::unsigned_integral<value_t>)
    {
        return src;
    }

    friend constexpr auto abs(fixed_t src) noexcept -> fixed_t
        requires(std::signed_integral<value_t>)
    {
        return src.value < 0 ? fixed_t{-src.value} : src;
    }

private:
    /// \returns out_t, widening or narrowing \p in with rounding mode if necessary
    template <is_fixed other_t, typename rounding_mode_t = fixed::default_rounding_mode_t>
    static constexpr auto convert(other_t other, rounding_mode_t rounding_mode = {}) noexcept -> fixed_t
    {
        static constexpr auto other_frac_bits = other_t::frac_bits;

        using promoted_value_t = fixed::promoted_t<fixed_t, other_t>::value_t;

        if constexpr (frac_bits > other_frac_bits)
        {
            // shift left using wider type
            return fixed_t{int_cast<value_t>(int_cast<promoted_value_t>(other.value) << (frac_bits - other_frac_bits))};
        }
        else if constexpr (other_frac_bits > frac_bits)
        {
            // right shift using rounding mode
            constexpr auto shift     = other_frac_bits - frac_bits;
            auto const     unshifted = int_cast<promoted_value_t>(other.value);
            auto const     shifted   = int_cast<promoted_value_t>(unshifted >> shift);
            return fixed_t{int_cast<value_t>(rounding_mode.shr(shifted, unshifted, shift))};
        }
        else
        {
            // no conversion necessary
            return fixed_t{int_cast<value_t>(other.value)};
        }
    }
};

/**
    division, stripped down for pipeline

    This is not a general implementation. The pipeline has one big divide, and this implementation considers its
    constraints:
        - all values are unsigned
        - all values are 64-bit
        - the output precision is at least as high as the dividend
    This simplifies the implementation compared to a general fixed/fixed with mixed signs and sizes.
*/
template <int out_frac_bits, int lhs_frac_bits, int rhs_frac_bits>
auto divide(fixed_t<uint64_t, lhs_frac_bits> lhs, fixed_t<uint64_t, rhs_frac_bits> rhs) noexcept
    -> fixed_t<uint64_t, out_frac_bits>
{
    constexpr auto total_shift = rhs_frac_bits + out_frac_bits - lhs_frac_bits;

    auto const divisor         = rhs.value;
    auto const divides_by_zero = 0 == divisor;
    if (divides_by_zero) [[unlikely]] { return {max<uint64_t>()}; }

    auto const dividend           = int_cast<uint128_t>(lhs.value) << total_shift;
    auto const quotient_overflows = (dividend >> 64) >= divisor;
    if (quotient_overflows) [[unlikely]] { return {max<uint64_t>()}; }

    auto [quotient, remainder] = div_u128_u64(dividend, divisor);

    quotient += remainder >= (divisor - remainder);

    return {quotient};
}

} // namespace crv
