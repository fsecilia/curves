// SPDX-License-Identifier: MIT

/// \file
/// \brief aligns final output radix
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>

namespace crv::spline {

/// aligns radix of the final evaluation step to match the precision of the output type
template <typename unpacked_field_t, typename t_scaled_int_t, auto align_exponent> struct radix_aligner_t
{
    using scaled_int_t = t_scaled_int_t;

    constexpr auto operator()(scaled_int_t const& accumulator, int_t radix) const noexcept -> unpacked_field_t
    {
        auto const aligned_accumulator
            = align_exponent(scaled_int_t{.mantissa = accumulator.mantissa, .exponent = accumulator.exponent + radix});

        return unpacked_field_t{
            .mantissa = aligned_accumulator.mantissa,
            .shift = -aligned_accumulator.exponent,
        };
    }
};

} // namespace crv::spline
