// SPDX-License-Identifier: MIT

/// \file
/// \brief cubic polynomial
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <array>
#include <ranges>

namespace crv::spline {

template <is_fixed coeff_t> using quadratic_polynomial_t = std::array<coeff_t, 3>;
template <is_fixed coeff_t> using cubic_polynomial_t = std::array<coeff_t, 4>;

/// single, fixed-point multiply and carry with normalized t
///
/// This is the standard fixed-point Horner's loop step.
struct mac_t
{
    template <is_fixed coeff_t, is_fixed normalized_t>
    constexpr auto operator()(coeff_t accumulator, normalized_t t, coeff_t coeff) const noexcept
        -> widened_t<typename coeff_t::value_t>
    {
        using wide_t = widened_t<typename coeff_t::value_t>;
        static constexpr auto rounding_bias = wide_t{1} << (normalized_t::frac_bits - 1);
        auto const product
            = (accumulator.value * static_cast<wide_t>(t.value) + rounding_bias) >> normalized_t::frac_bits;
        return product + coeff.value;
    }
};

/// fixed-length horner's loop using fold
///
/// This has no mitigation for overflow; it presumes the polynomial's intermediate steps were already checked at the
/// extrema.
///
/// \pre sum known to not overflow
template <auto mac> struct polynomial_evaluator_t
{
    template <std::ranges::range coeffs_t>
    constexpr auto operator()(auto t, coeffs_t const& coeffs) const noexcept -> std::ranges::range_value_t<coeffs_t>
    {
        using coeff_t = std::ranges::range_value_t<coeffs_t>;

        auto coeff = std::begin(coeffs);
        auto const end = std::end(coeffs);
        auto accumulator = *coeff++;
        while (coeff != end) accumulator = coeff_t::literal(narrow(mac(accumulator, t, *coeff++)));
        return accumulator;
    }
};

} // namespace crv::spline
