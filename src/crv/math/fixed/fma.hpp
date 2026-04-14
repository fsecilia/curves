// SPDX-License-Identifier: MIT

/// \file
/// \brief fixed point fma
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/saturation.hpp>
#include <crv/math/shifter.hpp>

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
    static_assert(is_signed == is_signed_v<multiplier_value_t> && is_signed == is_signed_v<addend_value_t>
                      && is_signed == is_signed_v<out_value_t>,
                  "fma operands must have identical signedness");

    // radix alignment shifts
    static constexpr auto max_frac      = std::max(product_t::frac_bits, addend_t::frac_bits);
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
    using sum_t          = int_by_bits_t<requested_sum_width, is_signed>;
    using unsigned_sum_t = make_unsigned_t<sum_t>;

    // prevent shifts larger than word size
    static constexpr auto actual_sum_width = static_cast<int_t>(sizeof(sum_t) * CHAR_BIT);
    static_assert(product_shift < actual_sum_width, "fma radix alignment shift exceeds container width");
    static_assert(addend_shift < actual_sum_width, "fma radix alignment shift exceeds container width");
    static_assert(out_shift <= 0 || out_shift < actual_sum_width,
                  "fma upscaling shift exceeds intermediate container width");
    static_assert(out_shift >= 0 || -out_shift < actual_sum_width,
                  "fma downscaling shift exceeds intermediate container width");

    // saturation limits by type
    static constexpr auto multiplicand_min = int_cast<sum_t>(min<multiplicand_value_t>());
    static constexpr auto multiplicand_max = int_cast<sum_t>(max<multiplicand_value_t>());
    static constexpr auto multiplier_min   = int_cast<sum_t>(min<multiplier_value_t>());
    static constexpr auto multiplier_max   = int_cast<sum_t>(max<multiplier_value_t>());
    static constexpr auto addend_min       = int_cast<sum_t>(min<addend_value_t>());
    static constexpr auto addend_max       = int_cast<sum_t>(max<addend_value_t>());
    static constexpr auto out_min          = int_cast<sum_t>(min<out_value_t>());
    static constexpr auto out_max          = int_cast<sum_t>(max<out_value_t>());

    // saturation limits by operation
    static constexpr auto product_min
        = std::min(static_cast<sum_t>(multiplicand_max) * static_cast<sum_t>(multiplier_min),
                   static_cast<sum_t>(multiplicand_min) * static_cast<sum_t>(multiplier_max));

    static constexpr auto product_max
        = std::max(static_cast<sum_t>(multiplicand_max) * static_cast<sum_t>(multiplier_max),
                   static_cast<sum_t>(multiplicand_min) * static_cast<sum_t>(multiplier_min));

    static constexpr auto aligned_product_min = static_cast<unsigned_sum_t>(product_min) << product_shift;
    static constexpr auto aligned_addend_min  = static_cast<unsigned_sum_t>(addend_min) << addend_shift;
    static constexpr auto aligned_product_max = static_cast<unsigned_sum_t>(product_max) << product_shift;
    static constexpr auto aligned_addend_max  = static_cast<unsigned_sum_t>(addend_max) << addend_shift;
    static constexpr auto unsigned_sum_min    = aligned_product_min + aligned_addend_min;
    static constexpr auto unsigned_sum_max    = aligned_product_max + aligned_addend_max;

    static constexpr auto sum_min      = static_cast<sum_t>(unsigned_sum_min);
    static constexpr auto sum_max      = static_cast<sum_t>(unsigned_sum_max);
    static constexpr auto narrowed_min
        = (out_shift < 0) ? ((sum_min >> -out_shift) - static_cast<sum_t>(is_signed)) : (sum_min << out_shift);
    static constexpr auto narrowed_max = (out_shift < 0) ? ((sum_max >> -out_shift) + 1) : (sum_max << out_shift);

    // determine if saturation is possible
    static constexpr auto saturate = overflow_policy == overflow_policy_t::saturate;
    static constexpr auto lower_overflow_possible
        = is_signed && (product_min < 0) && (addend_min < 0) && (sum_min >= 0);
    static constexpr auto upper_overflow_possible
        = is_signed ? (product_max > 0) && (addend_max > 0) && (sum_max < 0) : (unsigned_sum_max < aligned_product_max);
    static constexpr auto lower_saturation_possible = saturate && (lower_overflow_possible || narrowed_min < out_min);
    static constexpr auto upper_saturation_possible = saturate && (upper_overflow_possible || narrowed_max > out_max);

    /// fused multiply-add
    ///
    /// Calculates (a*b) + c at maximum intermediate precision before narrowing and rounding once.
    ///
    /// \pre all 3 terms must share signedness
    template <typename shifter_t = shifter_t<>>
    constexpr auto operator()(multiplicand_t multiplicand, multiplier_t multiplier, addend_t addend,
                              shifter_t shifter = shifter_t{}) noexcept -> out_t
    {
        // multiply
        auto const product = multiply(multiplicand, multiplier);

        // widen intermediates to common size
        auto const wide_product = int_cast<sum_t>(product.value);
        auto const wide_addend  = int_cast<sum_t>(addend.value);

        // align radix points
        auto aligned_product = static_cast<unsigned_sum_t>(wide_product);
        auto aligned_addend  = static_cast<unsigned_sum_t>(wide_addend);
        if constexpr (product_shift > 0) aligned_product <<= product_shift;
        if constexpr (addend_shift > 0) aligned_addend <<= addend_shift;

        // sum
        auto const sum = static_cast<sum_t>(aligned_product + aligned_addend);

        // shift to out_t precision
        auto const shifted_sum = shifter.template shift<out_shift>(sum);

        // optionally saturate upper
        if constexpr (upper_saturation_possible)
        {
            auto const overflows_upper = is_signed ? (wide_product > 0) && (wide_addend > 0) && (sum < 0)
                                                   : (static_cast<unsigned_sum_t>(sum) < aligned_product);

            if (overflows_upper || shifted_sum > out_max) return out_t::literal(max<out_value_t>());
        }

        // optionally saturate lower
        if constexpr (lower_saturation_possible)
        {
            auto const overflows_lower = is_signed && (wide_product < 0) && (wide_addend < 0) && (sum >= 0);

            if (overflows_lower || shifted_sum < out_min) return out_t::literal(min<out_value_t>());
        }

        return out_t::literal(static_cast<typename out_t::value_t>(shifted_sum));
    }
};

} // namespace crv
