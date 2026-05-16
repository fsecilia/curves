// SPDX-License-Identifier: MIT

/// \file
/// \brief float mantissa and exponent extraction
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/integer.hpp>
#include <crv/math/shifter.hpp>
#include <algorithm>
#include <bit>
#include <concepts>
#include <limits>

namespace crv {

/// holds floating point expressed as integer mantissa and exponent
template <typename t_mantissa_t> struct scaled_int_t
{
    using mantissa_t = t_mantissa_t;
    using exponent_t = int_t;

    mantissa_t mantissa;
    exponent_t exponent;

    constexpr auto operator<=>(scaled_int_t const&) const noexcept -> auto = default;
    constexpr auto operator==(scaled_int_t const&) const noexcept -> bool = default;
};

// extracts integer mantissa and exponent from float
template <std::floating_point t_scalar_t> struct float_extractor_t
{
    using scalar_t = t_scalar_t;

    static constexpr auto bit_count = int_t{sizeof(scalar_t) * CHAR_BIT};

    using unsigned_t = int_by_bits_t<bit_count, false>;
    using signed_t = int_by_bits_t<bit_count, true>;
    using scaled_int_t = scaled_int_t<signed_t>;

    // ieee constants
    static constexpr auto frac_bit_count = int_cast<signed_t>(std::numeric_limits<scalar_t>::digits - 1);
    static constexpr auto frac_mask = (unsigned_t{1} << frac_bit_count) - 1;
    static constexpr auto exp_bit_count = bit_count - 1 - frac_bit_count;
    static constexpr auto exponent_mask = (unsigned_t{1} << exp_bit_count) - 1;
    static constexpr auto exponent_bias = signed_t{(unsigned_t{1} << (exp_bit_count - 1)) - 1};
    static constexpr auto implicit_bit = unsigned_t{1} << frac_bit_count;

    /// \pre isfinite(value)
    constexpr auto operator()(scalar_t value) const noexcept -> scaled_int_t
    {
        auto const bits = std::bit_cast<unsigned_t>(value);

        // extract exponent
        auto const raw_exponent = (bits >> frac_bit_count) & exponent_mask;
        assert(raw_exponent != exponent_mask); // inf and nan are not supported
        if (raw_exponent == 0) return {}; // ftz; flush denormals to zero
        auto const exponent = int_cast<exponent_t>(int_cast<signed_t>(raw_exponent) - exponent_bias - frac_bit_count);

        return {.mantissa = extract_mantissa(bits), .exponent = exponent};
    }

private:
    using mantissa_t = scaled_int_t::mantissa_t;
    using exponent_t = scaled_int_t::exponent_t;

    constexpr auto extract_mantissa(unsigned_t bits) const noexcept -> mantissa_t
    {
        auto const raw_magnitude = (bits & frac_mask) | implicit_bit;

        auto mantissa = static_cast<mantissa_t>(raw_magnitude);
        assert(static_cast<unsigned_t>(mantissa) == raw_magnitude);

        auto const is_negative = (bits >> (bit_count - 1)) != 0;
        return is_negative ? -mantissa : mantissa;
    }
};

/// shifts integer mantissa to keep integer exponent within a range; saturates
template <int_t t_exponent_min, int_t t_exponent_max, auto shifter = saturating_shifter_t<>{}>
struct exponent_renormalizer_t
{
    static constexpr auto exponent_min = t_exponent_min;
    static constexpr auto exponent_max = t_exponent_max;

    template <typename scaled_int_t> constexpr auto operator()(scaled_int_t src) const noexcept -> scaled_int_t
    {
        auto const exponent_clamped = std::clamp(src.exponent, exponent_min, exponent_max);
        auto const left_shift = src.exponent - exponent_clamped;
        return {.mantissa = shifter.shift(src.mantissa, left_shift), .exponent = exponent_clamped};
    }
};

} // namespace crv
