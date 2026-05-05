// SPDX-License-Identifier: MIT

/// \file
/// \brief overflow check
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/abs.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/spline/polynomial.hpp>
#include <concepts>
#include <iterator>

namespace crv::spline::defect_detectors {

/// predicate to test for overflow in primal and tangent
///
/// This predicate tests intermediate and final calculations wide at extrema to see if they overflow.
template <std::floating_point real_t, is_fixed normalized_t> struct overflow_t
{
    /// \returns true if approximant overflows primal or tangent while evaluating anywhere over the interval
    template <is_fixed coeff_t>
    constexpr auto operator()(cubic_polynomial_t<coeff_t> const& coeffs) const noexcept -> bool
    {
        return (primal_can_overflow(coeffs) && primal_overflows(coeffs))
            || (tangent_can_overflow(coeffs) && tangent_overflows(coeffs));
    }

    /// fast-path, lightweight filter for primal overflow
    ///
    /// \returns false if primal absolutely cannot overflow
    template <is_fixed coeff_t>
    constexpr auto primal_can_overflow(cubic_polynomial_t<coeff_t> const& coeffs) const noexcept -> bool
    {
        auto const abs_sum = abs(widen(coeffs[0].value)) + abs(widen(coeffs[1].value)) + abs(widen(coeffs[2].value))
            + abs(widen(coeffs[3].value));
        return max<coeff_t>().value < abs_sum;
    }

    /// \returns true if polynomial overflows any intermediate calc while evaluating over the interval
    template <is_fixed coeff_t>
    constexpr auto primal_overflows(cubic_polynomial_t<coeff_t> const& coeffs) const noexcept -> bool
    {
        // test endpoints; this covers the linear intermediate in all cases
        if (operator()(coeffs, normalized_t{0})) return true;
        if (operator()(coeffs, normalized_t{1})) return true; // this won't compile if the type has no integer bits

        auto const a = from_fixed<real_t>(coeffs[0]);
        auto const b = from_fixed<real_t>(coeffs[1]);
        auto const c = from_fixed<real_t>(coeffs[2]);

        // find largest term
        if (a != 0.0)
        {
            // The cubic term is valid, so first check the absolute maximum the quadratic from the previous horner's
            // loop step could ever reach. If the full cubic is p(t) = at^3 + bt^2 + ct + d, the intermediate quadratic
            // from the previous step is q(t) = at^2 + bt + c. The peak is at q'(t) = 2at + b = 0 -> -b/2a
            auto const t_quadratic = -b / (2.0 * a);
            if (0.0 < t_quadratic && t_quadratic < 1.0 && operator()(coeffs, to_fixed<normalized_t>(t_quadratic)))
            {
                return true;
            }

            // The peaks of the final cubic are at p'(t) = 3at^2 + 2bt + c. Using the quadratic equation, the roots are
            // t = (-2b +/- sqrt(4b^2 - 12ac))/6a = (-b +/- sqrt(b^2 - 3ac))/3a
            auto const discriminant = b * b - 3.0 * a * c;
            auto const r3a = 1.0 / (3.0 * a);
            if (discriminant > 0.0)
            {
                using std::sqrt;
                auto const sqrt_discriminant = sqrt(discriminant);

                auto const t0 = (-b - sqrt_discriminant) * r3a;
                if (0.0 < t0 && t0 < 1.0 && operator()(coeffs, to_fixed<normalized_t>(t0))) return true;

                auto const t1 = (-b + sqrt_discriminant) * r3a;
                if (0.0 < t1 && t1 < 1.0 && operator()(coeffs, to_fixed<normalized_t>(t1))) return true;
            }
        }
        else if (b != 0.0)
        {
            // The cubic term is invalid, but the quadratic term is valid. The full quadratic is p(t) = bt^2 + ct + d.
            // The peak of the final quadratic is at p'(t) = 2bt + c = 0 -> -c/2b
            auto const t = -c / (2.0 * b);
            if (0.0 < t && t < 1.0 && operator()(coeffs, to_fixed<normalized_t>(t))) return true;
        }

        return false;
    }

    /// fast-path, lightweight filter for tangent overflow
    ///
    /// \returns false if tangent absolutely cannot overflow
    template <is_fixed coeff_t>
    constexpr auto tangent_can_overflow(cubic_polynomial_t<coeff_t> const& coeffs) const noexcept -> bool
    {
        auto const abs_sum
            = abs(3 * widen(coeffs[0].value)) + abs(2 * widen(coeffs[1].value)) + abs(widen(coeffs[2].value));
        return max<coeff_t>().value < abs_sum;
    }

    /// \returns true if polynomial overflows any intermediate calc while evaluating over the interval
    template <is_fixed coeff_t>
    constexpr auto tangent_overflows(cubic_polynomial_t<coeff_t> const& coeffs) const noexcept -> bool
    {
        // check 3*c0
        auto const c0 = coeffs[0];
        constexpr auto safe_c0_bound = max<coeff_t>().value / 3;
        if (c0.value < -safe_c0_bound || safe_c0_bound < c0.value) return true;

        // check 2*c1
        auto const c1 = coeffs[1];
        constexpr auto safe_c1_bound = max<coeff_t>().value / 2;
        if (c1.value < -safe_c1_bound || safe_c1_bound < c1.value) return true;

        // use primal predicate to check derivative polynomial
        auto const derivative = cubic_polynomial_t<coeff_t>{
            coeff_t::literal(3 * c0.value), coeff_t::literal(2 * c1.value), coeffs[2], coeff_t{0}};
        return primal_overflows(derivative);
    }

    /// returns true if polynomial overflows while evaluating at a particular location
    template <is_fixed coeff_t>
    constexpr auto operator()(cubic_polynomial_t<coeff_t> const& coeffs, normalized_t t) const noexcept -> bool
    {
        auto accumulator = coeffs[0];
        for (auto coeff = std::size_t{1}, coeff_count = std::size(coeffs); coeff < coeff_count; ++coeff)
        {
            // calc wide product
            auto wide_value = multiply(accumulator, t).value;

            // align radices
            wide_value >>= normalized_t::frac_bits;

            // sum
            wide_value += coeffs[coeff].value;

            // check for overflow
            if (wide_value < min<coeff_t>().value || max<coeff_t>().value < wide_value) return true;

            // accumulate
            accumulator.value = int_cast<typename coeff_t::value_t>(wide_value);
        }
        return false;
    }
};

} // namespace crv::spline::defect_detectors
