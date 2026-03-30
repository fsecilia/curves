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
using wider_t
    = fixed_t<sized_integer_t<std::max(sizeof(typename lhs_t::value_t), sizeof(typename rhs_t::value_t)) * 2,
                              std::is_signed_v<typename lhs_t::value_t> || std::is_signed_v<typename rhs_t::value_t>>,
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

    /// imports a semantic integer value, scaling to this type's fixed-point representation
    explicit constexpr fixed_t(value_t value) noexcept : value{int_cast<value_t>(value << frac_bits)} {}

    struct literal_t
    {
        value_t value;
    };
    explicit constexpr fixed_t(literal_t literal) noexcept : value{literal.value} {}

    /// value initializer - value is specified directly; it is not rescaled to frac_bits
    static constexpr auto literal(value_t value) noexcept -> fixed_t { return fixed_t{literal_t{value}}; }

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
        using other_value_t                   = typename other_t::value_t;
        static constexpr auto other_frac_bits = other_t::frac_bits;

        using intermediate_t
            = sized_integer_t<std::max(sizeof(value_t), sizeof(other_value_t)), std::is_signed_v<other_value_t>>;

        if constexpr (frac_bits > other_frac_bits)
        {
            static_assert((frac_bits - other_frac_bits) < sizeof(value_t) * CHAR_BIT, "left-shift overflow");

            constexpr auto shift = frac_bits - other_frac_bits;

            assert(other.value <= (std::numeric_limits<value_t>::max() >> shift) && "left-shift overflow");
            if constexpr (std::is_signed_v<other_value_t>)
            {
                assert(other.value >= (std::numeric_limits<value_t>::min() >> shift) && "left-shift underflow");
            }

            // shift left using wider type, letting int_cast catch any final truncation
            value = int_cast<value_t>(int_cast<intermediate_t>(other.value) << shift);
        }
        else if constexpr (other_frac_bits > frac_bits)
        {
            static_assert((other_frac_bits - frac_bits) < sizeof(other_value_t) * CHAR_BIT, "right-shift underflow");

            // right shift using rounding mode
            constexpr auto shift     = other_frac_bits - frac_bits;
            auto const     unshifted = rounding_mode.bias(int_cast<intermediate_t>(other.value), shift);
            auto const     shifted   = int_cast<intermediate_t>(unshifted >> shift);
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
    friend constexpr auto operator-(fixed_t src) noexcept -> fixed_t { return literal(-src.value); }

    // ----------------------------------------------------------------------------------------------------------------
    // Scalar Arithmetic
    // ----------------------------------------------------------------------------------------------------------------

    constexpr auto operator+=(value_t src) noexcept -> fixed_t& { return *this += fixed_t{src}; }

    friend constexpr auto operator+(fixed_t lhs, value_t rhs) noexcept -> fixed_t { return lhs += fixed_t{rhs}; }
    friend constexpr auto operator+(value_t lhs, fixed_t rhs) noexcept -> fixed_t { return rhs += fixed_t{lhs}; }

    constexpr auto operator-=(value_t src) noexcept -> fixed_t& { return *this -= fixed_t{src}; }

    friend constexpr auto operator-(fixed_t lhs, value_t rhs) noexcept -> fixed_t { return lhs -= fixed_t{rhs}; }
    friend constexpr auto operator-(value_t lhs, fixed_t const& rhs) noexcept -> fixed_t { return fixed_t{lhs} - rhs; }

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
        return fixed_t{lhs << frac_bits} / rhs.value;
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
        return *this = multiply<fixed_t>(*this, src, fixed::default_shr_rounding_mode);
    }

    constexpr auto operator/=(fixed_t src) noexcept -> fixed_t&
    {
        return *this = divide<fixed_t>(*this, src, fixed::default_div_rounding_mode);
    }

    friend constexpr auto operator+(fixed_t lhs, fixed_t rhs) noexcept -> fixed_t { return lhs += rhs; }
    friend constexpr auto operator-(fixed_t lhs, fixed_t rhs) noexcept -> fixed_t { return lhs -= rhs; }
    friend constexpr auto operator*(fixed_t lhs, fixed_t rhs) noexcept -> fixed_t { return lhs *= rhs; }
    friend constexpr auto operator/(fixed_t lhs, fixed_t rhs) noexcept -> fixed_t { return lhs /= rhs; }

    template <is_fixed other_t> friend constexpr auto operator*(fixed_t lhs, other_t rhs) noexcept -> fixed_t
    {
        return multiply<fixed_t>(lhs, rhs, fixed::default_shr_rounding_mode);
    }

    template <is_fixed other_t> friend constexpr auto operator/(fixed_t lhs, other_t rhs) noexcept -> fixed_t
    {
        return divide<fixed_t>(lhs, rhs, fixed::default_div_rounding_mode);
    }

    /// \returns wide product at higher precision
    template <is_fixed rhs_t>
    friend constexpr auto multiply(fixed_t lhs, rhs_t rhs) noexcept -> fixed::wider_t<fixed_t, rhs_t>
    {
        using result_t      = fixed::wider_t<fixed_t, rhs_t>;
        using wider_value_t = result_t::value_t;

        auto const product = int_cast<wider_value_t>(int_cast<wider_value_t>(lhs.value) * rhs.value);

        return result_t::literal(product);
    }

    /// \returns product, widened or narrowed to output type and rescaled to output precision using given rounding mode
    template <is_fixed out_t, is_fixed rhs_t, typename rounding_mode_t>
    friend constexpr auto multiply(fixed_t lhs, rhs_t rhs, rounding_mode_t rounding_mode) noexcept -> out_t
    {
        return out_t{multiply(lhs, rhs), rounding_mode};
    }

    /// \returns quotient, widened or narrowed to output type and rescaled to output precision using given rounding mode
    template <is_fixed out_t, is_fixed rhs_t, typename rounding_mode_t = fixed::default_div_rounding_mode_t>
    friend constexpr auto divide(fixed_t lhs, rhs_t rhs,
                                 rounding_mode_t rounding_mode = fixed::default_div_rounding_mode) noexcept -> out_t
    {
        using out_value_t = out_t::value_t;
        using lhs_value_t = fixed_t::value_t;
        using rhs_value_t = rhs_t::value_t;

        static constexpr auto shift = rhs_t::frac_bits - fixed_t::frac_bits + out_t::frac_bits;

        return divide<out_t>(lhs, rhs, rounding_mode, division::divide<out_value_t, lhs_value_t, rhs_value_t, shift>);
    }

    /// \returns quotient using divider
    template <is_fixed out_t, is_fixed rhs_t, typename rounding_mode_t, typename divider_t>
    friend constexpr auto divide(fixed_t lhs, rhs_t rhs, rounding_mode_t rounding_mode, divider_t divider) noexcept
        -> out_t
    {
        return out_t::literal(divider(lhs.value, rhs.value, rounding_mode));
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
        return src.value < 0 ? -src : src;
    }
};

} // namespace crv
