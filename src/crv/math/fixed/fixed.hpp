// SPDX-License-Identifier: MIT

/// \file
/// \brief fixed point integer type
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/division/divider.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/integer.hpp>
#include <crv/math/limits.hpp>
#include <crv/math/rounding_mode.hpp>
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

template <is_fixed lhs_t, is_fixed rhs_t>
using promoted_t = fixed_t<crv::promoted_t<typename lhs_t::value_t, typename rhs_t::value_t>,
                           std::max(lhs_t::frac_bits, rhs_t::frac_bits)>;

template <is_fixed lhs_t, is_fixed rhs_t>
using wider_t = fixed_t<crv::wider_t<crv::promoted_t<typename lhs_t::value_t, typename rhs_t::value_t>>,
                        lhs_t::frac_bits + rhs_t::frac_bits>;

inline constexpr auto default_shr_rounding_mode = rounding_modes::shr::truncate;
using default_shr_rounding_mode_t               = std::remove_cv_t<decltype(default_shr_rounding_mode)>;

inline constexpr auto default_div_rounding_mode = rounding_modes::div::truncate;
using default_div_rounding_mode_t               = std::remove_cv_t<decltype(default_div_rounding_mode)>;

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

    /// imports a semantic integer value, scaling to this type's fixed-point representation
    static constexpr auto from(value_t src) noexcept -> fixed_t
    {
        return fixed_t{static_cast<value_t>(src << frac_bits)};
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Copying
    // ----------------------------------------------------------------------------------------------------------------

    constexpr fixed_t(fixed_t const& src) noexcept                      = default;
    constexpr auto operator=(fixed_t const& other) noexcept -> fixed_t& = default;

    // ----------------------------------------------------------------------------------------------------------------
    // Conversions
    // ----------------------------------------------------------------------------------------------------------------

    /// converts from another fixed_t specialization, rescaling precision using rounding mode
    template <is_fixed other_t, typename rounding_mode_t = fixed::default_shr_rounding_mode_t>
    explicit constexpr fixed_t(other_t other, rounding_mode_t rounding_mode = {}) noexcept
    {
        static constexpr auto other_frac_bits = other_t::frac_bits;

        using promoted_value_t = fixed::promoted_t<fixed_t, other_t>::value_t;

        if constexpr (frac_bits > other_frac_bits)
        {
            // shift left using wider type
            value = int_cast<value_t>(int_cast<promoted_value_t>(other.value) << (frac_bits - other_frac_bits));
        }
        else if constexpr (other_frac_bits > frac_bits)
        {
            // right shift using rounding mode
            constexpr auto shift     = other_frac_bits - frac_bits;
            auto const     unshifted = rounding_mode.bias(int_cast<promoted_value_t>(other.value), shift);
            auto const     shifted   = int_cast<promoted_value_t>(unshifted >> shift);
            value                    = int_cast<value_t>(rounding_mode.carry(shifted, unshifted, shift));
        }
        else
        {
            // no conversion necessary
            value = int_cast<value_t>(other.value);
        }
    }

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
    // Scalar Arithmetic
    // ----------------------------------------------------------------------------------------------------------------

    constexpr auto operator+=(value_t src) noexcept -> fixed_t&
    {
        value += src;
        return *this;
    }

    friend constexpr auto operator+(fixed_t lhs, value_t rhs) noexcept -> fixed_t { return lhs += rhs; }
    friend constexpr auto operator+(value_t lhs, fixed_t rhs) noexcept -> fixed_t { return rhs += lhs; }

    constexpr auto operator-=(value_t src) noexcept -> fixed_t&
    {
        value -= src;
        return *this;
    }

    friend constexpr auto operator-(fixed_t lhs, value_t rhs) noexcept -> fixed_t { return lhs -= rhs; }
    friend constexpr auto operator-(value_t lhs, fixed_t const& rhs) noexcept -> fixed_t
    {
        return fixed_t{lhs - rhs.value};
    }

    constexpr auto operator*=(value_t src) noexcept -> fixed_t&
    {
        value *= src;
        return *this;
    }

    friend constexpr auto operator*(fixed_t lhs, value_t rhs) noexcept -> fixed_t { return lhs *= rhs; }
    friend constexpr auto operator*(value_t lhs, fixed_t rhs) noexcept -> fixed_t { return rhs *= lhs; }

    constexpr auto operator/=(value_t src) noexcept -> fixed_t&
    {
        value /= src;
        return *this;
    }

    friend constexpr auto operator/(fixed_t lhs, value_t rhs) noexcept -> fixed_t { return lhs /= rhs; }
    friend constexpr auto operator/(value_t lhs, fixed_t const& rhs) noexcept -> fixed_t
    {
        return fixed_t{lhs / rhs.value};
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Fixed Arithmetic
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
        return *this = multiply(*this, src, fixed::default_shr_rounding_mode);
    }

    constexpr auto operator/=(fixed_t src) noexcept -> fixed_t&
    {
        return *this = divide(*this, src, fixed::default_div_rounding_mode);
    }

    friend constexpr auto operator+(fixed_t lhs, fixed_t rhs) noexcept -> fixed_t { return lhs += rhs; }
    friend constexpr auto operator-(fixed_t lhs, fixed_t rhs) noexcept -> fixed_t { return lhs -= rhs; }
    friend constexpr auto operator*(fixed_t lhs, fixed_t rhs) noexcept -> fixed_t { return lhs *= rhs; }
    friend constexpr auto operator/(fixed_t lhs, fixed_t rhs) noexcept -> fixed_t { return lhs /= rhs; }

    template <is_fixed other_t> friend constexpr auto operator*(fixed_t lhs, other_t rhs) noexcept -> fixed_t
    {
        return multiply(lhs, rhs, fixed::default_shr_rounding_mode);
    }

    template <is_fixed other_t> friend constexpr auto operator/(fixed_t lhs, other_t rhs) noexcept -> fixed_t
    {
        return divide(lhs, rhs, fixed::default_shr_rounding_mode);
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

    /// \returns quotient, widened or narrowed to output type and rescaled to output precision using given rounding mode
    template <is_fixed out_t, is_fixed rhs_t, typename rounding_mode_t = fixed::default_div_rounding_mode_t>
    friend constexpr auto divide(fixed_t lhs, rhs_t rhs,
                                 rounding_mode_t rounding_mode = fixed::default_div_rounding_mode) noexcept -> out_t
    {
        using promoted_t = fixed::promoted_t<fixed_t, rhs_t>;
        using narrow_t   = promoted_t::value_t;

        static constexpr auto shift = rhs_t::frac_bits - fixed_t::frac_bits + out_t::frac_bits;

        return divide<out_t>(lhs, rhs, rounding_mode, division::divide<narrow_t, shift>);
    }

    /// \returns quotient using divider
    template <is_fixed out_t, is_fixed rhs_t, typename rounding_mode_t, typename divider_t>
    friend constexpr auto divide(fixed_t lhs, rhs_t rhs, rounding_mode_t rounding_mode, divider_t divider) noexcept
        -> out_t
    {
        return out_t{static_cast<typename out_t::value_t>(divider(lhs.value, rhs.value, rounding_mode))};
    }

    /// \returns quotient, narrowed to lhs type and rescaled to lhs precision using given rounding mode
    template <is_fixed rhs_t, typename rounding_mode_t>
    friend constexpr auto divide(fixed_t lhs, rhs_t rhs, rounding_mode_t rounding_mode) noexcept -> fixed_t
    {
        return divide<fixed_t, rhs_t, rounding_mode_t>(lhs, rhs, rounding_mode);
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
};

} // namespace crv
