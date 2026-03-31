// SPDX-License-Identifier: MIT

/// \file
/// \brief fixed point fma
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>

namespace crv {

/// fused multiply-add for fixed-point types
///
/// Calculates (a*b) + c at maximum intermediate precision before narrowing and rounding.
///
/// \pre signedness of all 3 terms must be the same
template <is_fixed out_t, is_fixed multipicand_t, is_fixed multiplier_t, is_fixed addend_t,
          typename rounding_mode_t = fixed::default_shr_rounding_mode_t, bool saturate = false>
constexpr auto fma(multipicand_t multiplicand, multiplier_t multiplier, addend_t addend,
                   rounding_mode_t rounding_mode = {}) noexcept -> out_t
{
    using multipicand_value_t = multipicand_t::value_t;
    using multiplier_value_t  = typename multiplier_t::value_t;
    using addend_value_t      = typename addend_t::value_t;

    // prevent unsafe implicit promotion
    static_assert((std::is_signed_v<multipicand_value_t> == std::is_signed_v<multiplier_value_t>)
                      && (std::is_signed_v<multiplier_value_t> == std::is_signed_v<addend_value_t>),
                  "fma operands must have identical signedness");
    constexpr auto is_signed = is_signed_v<multipicand_value_t>;

    auto const product = multiply(multiplicand, multiplier);
    using product_t    = decltype(product);

    constexpr auto max_frac = std::max(product_t::frac_bits, addend_t::frac_bits);

    // prevent negative shift UB
    static_assert(max_frac >= out_t::frac_bits, "fma upscaling is not supported");

    // radix alignment shifts
    constexpr auto product_shift = (max_frac > product_t::frac_bits) ? (max_frac - product_t::frac_bits) : 0;
    constexpr auto addend_shift  = (max_frac > addend_t::frac_bits) ? (max_frac - addend_t::frac_bits) : 0;

    constexpr auto max_product_bits  = (sizeof(typename product_t::value_t) * CHAR_BIT) + product_shift;
    constexpr auto max_addend_bits   = (sizeof(addend_value_t) * CHAR_BIT) + addend_shift;
    constexpr auto raw_required_bits = std::max(max_product_bits, max_addend_bits);
    constexpr auto required_bits     = raw_required_bits > 128 ? 128 : raw_required_bits;

    using sum_value_t = int_by_bits_t<required_bits, is_signed>;

    static_assert(product_shift < sizeof(sum_value_t) * CHAR_BIT, "fma radix alignment shift exceeds container width");
    static_assert(addend_shift < sizeof(sum_value_t) * CHAR_BIT, "fma radix alignment shift exceeds container width");

    // align radix points
    auto aligned_product = int_cast<sum_value_t>(product.value);
    auto aligned_addend  = int_cast<sum_value_t>(addend.value);
    if constexpr (product_shift > 0) aligned_product <<= product_shift;
    else if constexpr (addend_shift > 0) aligned_addend <<= addend_shift;

    auto sum = aligned_product + aligned_addend;

    // optionally shift down to out_t precision
    constexpr auto out_shift = max_frac - out_t::frac_bits;
    if constexpr (out_shift)
    {
        auto const unshifted = rounding_mode.bias(sum, out_shift);
        auto const shifted   = unshifted >> out_shift;
        sum                  = rounding_mode.carry(shifted, unshifted, out_shift);
    }

    // saturation
    if constexpr (saturate)
    {
        using out_val_t    = typename out_t::value_t;
        constexpr auto min = int_cast<sum_value_t>(crv::min<out_val_t>());
        constexpr auto max = int_cast<sum_value_t>(crv::max<out_val_t>());

        if (sum > max) return out_t::literal(crv::max<out_val_t>());
        if (sum < min) return out_t::literal(crv::min<out_val_t>());
    }

    return out_t::literal(int_cast<typename out_t::value_t>(sum));
}

/// ergonomic wrapper for saturating fma with specified output type
template <is_fixed out_t, bool saturate, is_fixed multiplicand_t, is_fixed multiplier_t, is_fixed addend_t,
          typename rounding_mode_t = fixed::default_shr_rounding_mode_t>
constexpr auto fma(multiplicand_t multiplicand, multiplier_t multiplier, addend_t addend,
                   rounding_mode_t rounding_mode = {}) noexcept -> out_t
{
    return fma<out_t, multiplicand_t, multiplier_t, addend_t, rounding_mode_t, saturate>(multiplicand, multiplier,
                                                                                         addend, rounding_mode);
}

/// ergonomic wrapper for saturating fma with deduced output type
template <bool saturate, is_fixed multiplicand_t, is_fixed multiplier_t, is_fixed addend_t,
          typename rounding_mode_t = fixed::default_shr_rounding_mode_t, typename out_t = multiplicand_t>
constexpr auto fma(multiplicand_t multiplicand, multiplier_t multiplier, addend_t addend,
                   rounding_mode_t rounding_mode = {}) noexcept -> out_t
{
    return fma<out_t, multiplicand_t, multiplier_t, addend_t, rounding_mode_t, saturate>(multiplicand, multiplier,
                                                                                         addend, rounding_mode);
}

} // namespace crv
