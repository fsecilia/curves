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
#include <climits>
#include <type_traits>

namespace crv {

template <integral value_t, int frac_bits> struct fixed_t;

// --------------------------------------------------------------------------------------------------------------------
// Concepts
// --------------------------------------------------------------------------------------------------------------------

template <typename type_t> struct is_fixed_f : std::false_type
{};

template <integral value_t, int frac_bits> struct is_fixed_f<fixed_t<value_t, frac_bits>> : std::true_type
{};

template <typename type_t> static constexpr auto is_fixed_v = is_fixed_f<std::remove_cv_t<type_t>>::value;

template <typename type_t>
concept is_fixed = is_fixed_v<type_t>;

// --------------------------------------------------------------------------------------------------------------------
// Type Traits
// --------------------------------------------------------------------------------------------------------------------

namespace fixed {

template <is_fixed lhs_t, is_fixed rhs_t>
using product_t
    = fixed_t<int_by_bytes_t<std::max(sizeof(typename lhs_t::value_t), sizeof(typename rhs_t::value_t)) * 2,
                             std::is_signed_v<typename lhs_t::value_t> || std::is_signed_v<typename rhs_t::value_t>>,
              lhs_t::frac_bits + rhs_t::frac_bits>;

inline constexpr auto default_shr_rounding_mode = rounding_modes::shr::truncate;
using default_shr_rounding_mode_t               = std::remove_cv_t<decltype(default_shr_rounding_mode)>;

inline constexpr auto default_div_rounding_mode = rounding_modes::div::truncate;
using default_div_rounding_mode_t               = std::remove_cv_t<decltype(default_div_rounding_mode)>;

} // namespace fixed

// --------------------------------------------------------------------------------------------------------------------
// fixed_t
// --------------------------------------------------------------------------------------------------------------------

template <is_fixed fixed_t> constexpr auto literal(typename fixed_t::value_t value) -> fixed_t
{
    return fixed_t::literal(value);
}

template <integral value_t, int frac_bits> constexpr auto literal(value_t value) -> fixed_t<value_t, frac_bits>
{
    return literal<fixed_t<value_t, frac_bits>>(value);
}

/// fixed-point arithmetic type with statically-configurable precision
template <integral t_value_t, int t_frac_bits> struct fixed_t
{
    using value_t                   = t_value_t;
    static constexpr auto frac_bits = t_frac_bits;

    value_t value;

    // ----------------------------------------------------------------------------------------------------------------
    // Construction
    // ----------------------------------------------------------------------------------------------------------------

    /// default initializer matches underlying
    constexpr fixed_t() = default;

    /// imports a semantic integer value, scaling to this type's fixed-point representation
    ///
    /// \pre shift must be smaller than bit width of underlying
    explicit constexpr fixed_t(value_t value) noexcept
        requires(frac_bits < sizeof(value_t) * CHAR_BIT)
        : value{int_cast<value_t>(value << frac_bits)}
    {}

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
    explicit constexpr fixed_t(other_t const& other, rounding_mode_t rounding_mode = {}) noexcept
    {
        using other_value_t                   = typename other_t::value_t;
        static constexpr auto other_frac_bits = other_t::frac_bits;

        using intermediate_t
            = int_by_bytes_t<std::max(sizeof(value_t), sizeof(other_value_t)), std::is_signed_v<other_value_t>>;

        if constexpr (frac_bits > other_frac_bits)
        {
            static_assert((frac_bits - other_frac_bits) < sizeof(value_t) * CHAR_BIT, "left-shift overflow");

            constexpr auto shift = frac_bits - other_frac_bits;

            assert(other.value <= (std::numeric_limits<value_t>::max() >> shift) && "left-shift overflow");
            if constexpr (std::is_signed_v<other_value_t>)
            {
                assert(other.value >= (std::numeric_limits<value_t>::min() >> shift) && "left-shift underflow");
            }

            // shift left using widened type, letting int_cast catch any final truncation
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
    constexpr auto operator-=(value_t src) noexcept -> fixed_t& { return *this -= fixed_t{src}; }

    constexpr auto operator*=(value_t src) noexcept -> fixed_t&
    {
        value *= src;
        return *this;
    }

    constexpr auto operator/=(value_t src) noexcept -> fixed_t&
    {
        value /= src;
        return *this;
    }

    constexpr auto operator%=(value_t src) noexcept -> fixed_t&
    {
        value %= (src << frac_bits);
        return *this;
    }

    constexpr auto operator>>=(value_t src) noexcept -> fixed_t&
    {
        value >>= src;
        return *this;
    }

    constexpr auto operator<<=(value_t src) noexcept -> fixed_t&
    {
        value <<= src;
        return *this;
    }

    friend constexpr auto operator+(fixed_t lhs, value_t rhs) noexcept -> fixed_t { return lhs += fixed_t{rhs}; }
    friend constexpr auto operator+(value_t lhs, fixed_t rhs) noexcept -> fixed_t { return rhs += fixed_t{lhs}; }

    friend constexpr auto operator-(fixed_t lhs, value_t rhs) noexcept -> fixed_t { return lhs -= fixed_t{rhs}; }
    friend constexpr auto operator-(value_t lhs, fixed_t rhs) noexcept -> fixed_t { return fixed_t{lhs} - rhs; }

    friend constexpr auto operator*(fixed_t lhs, value_t rhs) noexcept -> fixed_t { return lhs *= rhs; }
    friend constexpr auto operator*(value_t lhs, fixed_t rhs) noexcept -> fixed_t { return rhs *= lhs; }

    friend constexpr auto operator/(fixed_t lhs, value_t rhs) noexcept -> fixed_t { return lhs /= rhs; }
    friend constexpr auto operator/(value_t lhs, fixed_t rhs) noexcept -> fixed_t
    {
        return fixed_t::literal(int_cast<value_t>((widened_t<value_t>(lhs) << frac_bits * 2) / rhs.value));
    }

    friend constexpr auto operator%(fixed_t lhs, value_t rhs) noexcept -> fixed_t { return lhs %= rhs; }

    friend constexpr auto operator>>(fixed_t lhs, value_t rhs) noexcept -> fixed_t { return lhs >>= rhs; }
    friend constexpr auto operator<<(fixed_t lhs, value_t rhs) noexcept -> fixed_t { return lhs <<= rhs; }

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

    template <is_fixed other_t> constexpr auto operator%=(other_t src) noexcept -> fixed_t&
    {
        return *this = mod(*this, src);
    }

    friend constexpr auto operator+(fixed_t lhs, fixed_t rhs) noexcept -> fixed_t { return lhs += rhs; }
    friend constexpr auto operator-(fixed_t lhs, fixed_t rhs) noexcept -> fixed_t { return lhs -= rhs; }
    friend constexpr auto operator*(fixed_t lhs, fixed_t rhs) noexcept -> fixed_t { return lhs *= rhs; }
    friend constexpr auto operator/(fixed_t lhs, fixed_t rhs) noexcept -> fixed_t { return lhs /= rhs; }
    friend constexpr auto operator%(fixed_t lhs, fixed_t rhs) noexcept -> fixed_t { return lhs %= rhs; }

    template <is_fixed other_t> friend constexpr auto operator*(fixed_t lhs, other_t rhs) noexcept -> fixed_t
    {
        return multiply<fixed_t>(lhs, rhs, fixed::default_shr_rounding_mode);
    }

    template <is_fixed other_t> friend constexpr auto operator/(fixed_t lhs, other_t rhs) noexcept -> fixed_t
    {
        return divide<fixed_t>(lhs, rhs, fixed::default_div_rounding_mode);
    }

    template <is_fixed other_t> friend constexpr auto operator%(fixed_t lhs, other_t rhs) noexcept -> fixed_t
    {
        return mod(lhs, rhs);
    }

    /// \returns wide product at higher precision
    template <is_fixed rhs_t>
    friend constexpr auto multiply(fixed_t lhs, rhs_t rhs) noexcept -> fixed::product_t<fixed_t, rhs_t>
    {
        using result_t      = fixed::product_t<fixed_t, rhs_t>;
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

    /// calcs remainder of lhs/rhs
    template <is_fixed out_t, is_fixed rhs_t, typename rounding_mode_t = fixed::default_shr_rounding_mode_t>
    friend constexpr auto mod(fixed_t lhs, rhs_t rhs, rounding_mode_t rounding_mode = {}) noexcept -> out_t
    {
        // widen
        using wide_t  = int_by_bytes_t<std::max(sizeof(value_t), sizeof(typename rhs_t::value_t)) * 2,
                                       std::is_signed_v<value_t> || std::is_signed_v<typename rhs_t::value_t>>;
        auto lhs_wide = int_cast<wide_t>(lhs.value);
        auto rhs_wide = int_cast<wide_t>(rhs.value);

        // align radix points
        constexpr auto max_frac = std::max(frac_bits, rhs_t::frac_bits);
        if constexpr (max_frac > frac_bits) lhs_wide <<= (max_frac - frac_bits);
        if constexpr (max_frac > rhs_t::frac_bits) rhs_wide <<= (max_frac - rhs_t::frac_bits);

        // modulo
        auto const remainder = lhs_wide % rhs_wide;

        // scale to output precision
        constexpr auto out_shift = max_frac - out_t::frac_bits;
        if constexpr (out_shift > 0)
        {
            auto const unshifted = rounding_mode.bias(remainder, out_shift);
            auto const shifted   = unshifted >> out_shift;
            return out_t::literal(
                int_cast<typename out_t::value_t>(rounding_mode.carry(shifted, unshifted, out_shift)));
        }
        else
        {
            return out_t::literal(int_cast<typename out_t::value_t>(remainder << -out_shift));
        }
    }

    /// calcs remainder of lhs/rhs, defaulting to dividend's type and precision
    template <is_fixed divisor_t> friend constexpr auto mod(fixed_t dividend, divisor_t divisor) noexcept -> fixed_t
    {
        return mod<fixed_t>(dividend, divisor, fixed::default_shr_rounding_mode);
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Rounding and Extraction
    // ----------------------------------------------------------------------------------------------------------------

    friend constexpr auto ceil(fixed_t src) noexcept -> fixed_t
    {
        static_assert(frac_bits < sizeof(value_t) * CHAR_BIT, "ceil() undefined for pure fractional types");
        if constexpr (frac_bits == 0) return src;
        else
        {
            auto result = floor(src);
            if (result.value != src.value)
            {
                result = literal(int_cast<value_t>(result.value + (value_t{1} << frac_bits)));
            }
            return result;
        }
    }

    friend constexpr auto floor(fixed_t src) noexcept -> fixed_t
    {
        static_assert(frac_bits < sizeof(value_t) * CHAR_BIT, "floor() undefined for pure fractional types");
        if constexpr (frac_bits == 0) return src;
        else
        {
            using unsigned_t        = std::make_unsigned_t<value_t>;
            constexpr auto int_mask = ~((unsigned_t{1} << frac_bits) - 1);
            return literal(static_cast<value_t>(static_cast<unsigned_t>(src.value) & int_mask));
        }
    }

    /// \returns strictly positive fractional component
    friend constexpr auto frac(fixed_t src) noexcept -> fixed_t
    {
        static_assert(frac_bits < sizeof(value_t) * CHAR_BIT, "frac() undefined for pure fractional types.");
        if constexpr (frac_bits == 0) return literal(0);
        else
        {
            using unsigned_t         = std::make_unsigned_t<value_t>;
            constexpr auto frac_mask = (unsigned_t{1} << frac_bits) - 1;
            return literal(int_cast<value_t>(static_cast<unsigned_t>(src.value) & frac_mask));
        }
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

namespace std {

template <typename value_t, int frac_bits>
struct numeric_limits<crv::fixed_t<value_t, frac_bits>> : numeric_limits<value_t>
{
    using base_t  = numeric_limits<value_t>;
    using fixed_t = crv::fixed_t<value_t, frac_bits>;

    static constexpr bool is_specialized = true;

    // fixed-point represents fractions, but does so exactly
    static constexpr bool is_integer = false;
    static constexpr bool is_exact   = true;

    // fixed-point lacks float special values
    static constexpr bool has_infinity      = false;
    static constexpr bool has_quiet_NaN     = false;
    static constexpr bool has_signaling_NaN = false;

    // smallest strictly positive value is 1 in the underlying
    static constexpr auto min() noexcept -> fixed_t { return fixed_t::literal(1); }

    // max and lowest wrap the underlying integer's bounds
    static constexpr auto max() noexcept -> fixed_t { return fixed_t::literal(base_t::max()); }
    static constexpr auto lowest() noexcept -> fixed_t { return fixed_t::literal(base_t::lowest()); }

    // the difference between 1.0 and the next representable value is always 1
    static constexpr auto epsilon() noexcept -> fixed_t { return fixed_t::literal(1); }
    static constexpr auto round_error() noexcept -> fixed_t { return fixed_t::literal(1); }

    // Return zero-initialized representations for unsupported float concepts
    static constexpr auto infinity() noexcept -> fixed_t { return fixed_t::literal(0); }
    static constexpr auto quiet_NaN() noexcept -> fixed_t { return fixed_t::literal(0); }
    static constexpr auto signaling_NaN() noexcept -> fixed_t { return fixed_t::literal(0); }
    static constexpr auto denorm_min() noexcept -> fixed_t { return min(); }
};

} // namespace std
