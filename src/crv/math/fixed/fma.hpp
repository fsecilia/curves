// SPDX-License-Identifier: MIT

/// \file
/// \brief fixed point fma
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/saturation.hpp>

namespace crv {

/// fused multiply-add for fixed-point types
template <is_fixed out_t, is_fixed multiplicand_t, is_fixed multiplier_t, is_fixed addend_t,
          overflow_policy_t overflow_policy = overflow_policy_t::wrap>
struct fma_t
{
    // wide intermediate
    using product_t = fixed::product_t<multiplicand_t, multiplier_t>;

    // value types
    using multiplicand_value_t = multiplicand_t::value_t;
    using multiplier_value_t   = multiplier_t::value_t;
    using product_value_t      = product_t::value_t;
    using addend_value_t       = addend_t::value_t;
    using out_value_t          = out_t::value_t;

    // prevent mixed signedness
    static constexpr auto is_signed = is_signed_v<multiplicand_value_t>;
    static_assert(is_signed == std::is_signed_v<multiplier_value_t> && is_signed == std::is_signed_v<addend_value_t>,
                  "fma operands must have identical signedness");

    // prevent left shifts after sum
    static constexpr auto max_frac = std::max(product_t::frac_bits, addend_t::frac_bits);
    // static_assert(max_frac >= out_t::frac_bits, "fma upscaling is not supported");

    // radix alignment shifts
    static constexpr auto product_shift = (max_frac > product_t::frac_bits) ? (max_frac - product_t::frac_bits) : 0;
    static constexpr auto addend_shift  = (max_frac > addend_t::frac_bits) ? (max_frac - addend_t::frac_bits) : 0;
    static constexpr auto out_shift     = static_cast<int_t>(out_t::frac_bits) - static_cast<int_t>(max_frac);

    // determine width of sum
    static constexpr auto max_product_bits    = (sizeof(product_value_t) * CHAR_BIT) + product_shift;
    static constexpr auto max_addend_bits     = (sizeof(addend_value_t) * CHAR_BIT) + addend_shift;
    static constexpr auto max_aligned_bits    = std::max(max_product_bits, max_addend_bits);
    static constexpr auto required_sum_width  = (out_shift < 0) ? max_aligned_bits : (max_aligned_bits + out_shift);
    static constexpr auto requested_sum_width = std::min(required_sum_width, 128ul);

    // wide intermediate sum
    using sum_value_t = int_by_bits_t<requested_sum_width, is_signed>;

    // prevent shifts larger than word size
    static constexpr auto actual_sum_width = sizeof(sum_value_t) * CHAR_BIT;
    static_assert(product_shift < actual_sum_width, "fma radix alignment shift exceeds container width");
    static_assert(addend_shift < actual_sum_width, "fma radix alignment shift exceeds container width");
    static_assert(out_shift <= 0 || out_shift < static_cast<int>(actual_sum_width),
                  "fma upscaling shift exceeds intermediate container width");
    static_assert(out_shift >= 0 || -out_shift < static_cast<int>(actual_sum_width),
                  "fma downscaling shift exceeds intermediate container width");

    // saturation limits by type
    static constexpr auto multiplicand_min = int_cast<sum_value_t>(min<multiplicand_value_t>());
    static constexpr auto multiplicand_max = int_cast<sum_value_t>(max<multiplicand_value_t>());
    static constexpr auto multiplier_min   = int_cast<sum_value_t>(min<multiplier_value_t>());
    static constexpr auto multiplier_max   = int_cast<sum_value_t>(max<multiplier_value_t>());
    static constexpr auto addend_min       = int_cast<sum_value_t>(min<addend_value_t>());
    static constexpr auto addend_max       = int_cast<sum_value_t>(max<addend_value_t>());
    static constexpr auto out_min          = int_cast<sum_value_t>(min<out_value_t>());
    static constexpr auto out_max          = int_cast<sum_value_t>(max<out_value_t>());

    // saturation limits by operation
    static constexpr auto product_min  = std::min(multiplicand_max * multiplier_min, multiplicand_min* multiplier_max);
    static constexpr auto product_max  = std::max(multiplicand_max * multiplier_max, multiplicand_min* multiplier_min);
    static constexpr auto sum_min      = (product_min << product_shift) + (addend_min << addend_shift);
    static constexpr auto sum_max      = (product_max << product_shift) + (addend_max << addend_shift);
    static constexpr auto narrowed_min = (out_shift < 0) ? ((sum_min >> -out_shift) - 1) : (sum_min << out_shift);
    static constexpr auto narrowed_max = (out_shift < 0) ? ((sum_max >> -out_shift) + 1) : (sum_max << out_shift);

    // determine if saturation is possible
    static constexpr auto saturate                  = overflow_policy == overflow_policy_t::saturate;
    static constexpr auto lower_saturation_possible = saturate && (narrowed_min < out_min);
    static constexpr auto upper_saturation_possible = saturate && (narrowed_max > out_max);

    /// fused multiply-add
    ///
    /// Calculates (a*b) + c at maximum intermediate precision before narrowing and rounding once.
    ///
    /// \pre all 3 terms must share signedness
    template <typename rounding_mode_t = fixed::default_shr_rounding_mode_t>
    constexpr auto operator()(multiplicand_t multiplicand, multiplier_t multiplier, addend_t addend,
                              rounding_mode_t rounding_mode = {}) noexcept -> out_t
    {
        // multiply
        auto const product = multiply(multiplicand, multiplier);

        // align radix points
        auto aligned_product = int_cast<sum_value_t>(product.value);
        auto aligned_addend  = int_cast<sum_value_t>(addend.value);
        if constexpr (product_shift > 0) aligned_product <<= product_shift;
        else if constexpr (addend_shift > 0) aligned_addend <<= addend_shift;

        // sum
        auto sum = aligned_product + aligned_addend;

        // optionally shift to out_t precision
        if constexpr (out_shift < 0)
        {
            // downscaling requires rounding
            constexpr auto shr       = -out_shift;
            auto const     unshifted = rounding_mode.bias(sum, shr);
            auto const     shifted   = unshifted >> shr;
            sum                      = rounding_mode.carry(shifted, unshifted, shr);
        }
        else if constexpr (out_shift > 0)
        {
            // upscaling requires no rounding
            sum <<= out_shift;
        }

        // optionally saturate upper
        if constexpr (upper_saturation_possible)
        {
            if (sum > out_max) return out_t::literal(max<out_value_t>());
        }

        // optionally saturate lower
        if constexpr (lower_saturation_possible)
        {
            if (sum < out_min) return out_t::literal(min<out_value_t>());
        }

        return out_t::literal(int_cast<typename out_t::value_t>(sum));
    }
};

} // namespace crv
