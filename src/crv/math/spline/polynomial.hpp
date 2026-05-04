// SPDX-License-Identifier: MIT

/// \file
/// \brief cubic polynomial
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <array>

namespace crv::spline {

constexpr auto cubic_coeff_count = 4;
template <is_fixed coeff_t> using cubic_polynomial_t = std::array<coeff_t, cubic_coeff_count>;

/// single, fixed-point multiply and carry with normalized t
///
/// This is a standard fixed-point Horner's loop step. It has no mitigation for overflow; it presumes the polynomial's
/// intermediate steps were already checked at the extrema.
///
/// \pre sum known to not overflow
struct fast_mac_t
{
    template <is_fixed coeff_t, is_fixed normalized_t>
    constexpr auto operator()(coeff_t accumulator, normalized_t t, coeff_t coeff) const noexcept -> coeff_t
    {
        using narrow_t = typename coeff_t::value_t;
        using wide_t = widened_t<narrow_t>;
        static constexpr auto rounding_bias = wide_t{1} << (normalized_t::frac_bits - 1);
        auto const wide_product = static_cast<wide_t>(accumulator.value) * t.value + rounding_bias;
        auto const narrow_product = static_cast<narrow_t>(wide_product >> normalized_t::frac_bits);
        return coeff_t::literal(narrow_product + coeff.value);
    }
};

} // namespace crv::spline
