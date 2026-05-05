// SPDX-License-Identifier: MIT

/// \file
/// \brief polynomial overflow check
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/abs.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/spline/polynomial.hpp>
#include <crv/ranges.hpp>
#include <concepts>
#include <iterator>

namespace crv::spline::defect_detectors {

/// predicate to test for overflow in primal and tangent of fixed-point polynomial
///
/// This predicate tests intermediate and final calculations wide at extrema to see if they overflow.
template <std::floating_point real_t, is_fixed normalized_t, auto mac> struct overflow_t
{
    /// \returns true if polynomial overflows primal or tangent while evaluating anywhere over the interval
    template <is_fixed coeff_t>
    constexpr auto operator()(cubic_polynomial_t<coeff_t> const& cubic) const noexcept -> bool
    {
        return overflows(cubic) || derivative_overflows(cubic);
    }

    /// returns true if polynomial overflows any intermediate or final calc when evaluating at a particular location
    template <std::ranges::range coeffs_t>
    constexpr auto operator()(coeffs_t const& coeffs, normalized_t t) const noexcept -> bool
        requires(is_fixed<std::ranges::range_value_t<coeffs_t>>)
    {
        return overflows_at(coeffs, t);
    }

private:
    // true if derivative of polynomial overflows any intermediate calc while evaluating over the interval
    template <is_fixed coeff_t>
    constexpr auto derivative_overflows(cubic_polynomial_t<coeff_t> const& cubic) const noexcept -> bool
    {
        // check if taking derivative will overflow
        if (overflows(3 * widen(cubic[0].value)) || overflows(2 * widen(cubic[1].value))) return true;

        // check derivative
        return overflows(quadratic_polynomial_t{3 * cubic[0], 2 * cubic[1], cubic[2]});
    }

    // true if wide value exceeds narrow range
    template <typename wide_value_t> constexpr auto overflows(wide_value_t const wide_value) const noexcept -> bool
    {
        using narrow_value_t = narrowed_t<wide_value_t>;
        return wide_value < min<narrow_value_t>() || max<narrow_value_t>() < wide_value;
    }

    // true if polynomial overflows any intermediate calc while evaluating over the interval
    template <is_fixed coeff_t>
    constexpr auto overflows(linear_polynomial_t<coeff_t> const& coeffs) const noexcept -> bool
    {
        if (coeffs[0] == 0) return false;

        // linear term only needs to check endpoint
        return endpoint_overflows(coeffs);
    }

    // true if polynomial overflows any intermediate calc while evaluating over the interval
    template <is_fixed coeff_t>
    constexpr auto overflows(quadratic_polynomial_t<coeff_t> const& coeffs) const noexcept -> bool
    {
        if (coeffs[0] == 0) return overflows(linear_polynomial_t{coeffs[1], coeffs[2]});

        // check envelope to see if overflow is even possible
        auto const abs_sum = abs(widen(coeffs[0].value)) + abs(widen(coeffs[1].value)) + abs(widen(coeffs[2].value));
        if (abs_sum <= max<coeff_t>().value) return false;
        if (endpoint_overflows(coeffs)) return true;

        auto const a = from_fixed<real_t>(coeffs[0]);
        auto const b = from_fixed<real_t>(coeffs[1]);

        // quadratic is p(t) = bt^2 + ct + d; peak is at p'(t) = 2at + b = 0 -> -b/2a
        auto const t = -b / (2.0 * a);
        return 0.0 < t && t < 1.0 && overflows_at(coeffs, to_fixed(t));
    }

    // true if polynomial overflows any intermediate calc while evaluating over the interval
    template <is_fixed coeff_t>
    constexpr auto overflows(cubic_polynomial_t<coeff_t> const& coeffs) const noexcept -> bool
    {
        if (coeffs[0] == 0) return overflows(quadratic_polynomial_t<coeff_t>{coeffs[1], coeffs[2], coeffs[3]});

        // check envelope to see if overflow is even possible
        auto const abs_sum = abs(widen(coeffs[0].value)) + abs(widen(coeffs[1].value)) + abs(widen(coeffs[2].value))
            + abs(widen(coeffs[3].value));
        if (abs_sum <= max<coeff_t>().value) return false;
        if (endpoint_overflows(coeffs)) return true;

        // full cubic is p(t) = at^3 + bt^2 + ct + d; intermediate quadratic is q(t) = at^2 + bt + c
        auto const a = from_fixed<real_t>(coeffs[0]);
        auto const b = from_fixed<real_t>(coeffs[1]);
        auto const c = from_fixed<real_t>(coeffs[2]);

        // check absolute maximum the quadratic from the previous horner's loop step could ever reach
        //
        // peak is at q'(t) = 2at + b = 0 -> -b/2a
        auto const t_quadratic = -b / (2.0 * a);
        if (0.0 < t_quadratic && t_quadratic < 1.0 && overflows_at(coeffs, to_fixed(t_quadratic))) { return true; }

        // peaks of final cubic are at p'(t) = 3at^2 + 2bt + c
        // from the quadratic equation, roots are at t = (-2b +/- sqrt(4b^2 - 12ac))/6a = (-b +/- sqrt(b^2 - 3ac))/3a
        auto const discriminant = b * b - 3.0 * a * c;
        auto const r3a = 1.0 / (3.0 * a);
        if (discriminant > 0.0)
        {
            using std::sqrt;
            auto const sqrt_discriminant = sqrt(discriminant);

            auto const t0 = (-b - sqrt_discriminant) * r3a;
            if (0.0 < t0 && t0 < 1.0 && overflows_at(coeffs, to_fixed(t0))) return true;

            auto const t1 = (-b + sqrt_discriminant) * r3a;
            if (0.0 < t1 && t1 < 1.0 && overflows_at(coeffs, to_fixed(t1))) return true;
        }

        return false;
    }

    // checks if evaluating at right endpoint overflows
    //
    // At the right endpoint, when t=1, the result is just the sum of the coefficients; multiplying by 1, even in
    // fixed-point, does not change the result, so just sum wide.
    //
    // The left endpoint never needs to be checked; it is only ever the final coeff which cannot overflow.
    template <std::ranges::range coeffs_t>
    constexpr auto endpoint_overflows(coeffs_t const& coeffs) const noexcept -> bool
        requires(is_fixed<std::ranges::range_value_t<coeffs_t>>)
    {
        auto coeff = std::begin(coeffs);
        auto const coeffs_end = std::end(coeffs);

        auto wide_accumulator = widen(coeff++->value);
        while (coeff != coeffs_end)
        {
            wide_accumulator += coeff++->value;
            if (overflows(wide_accumulator)) return true;
        }
        return false;
    }

    // true if polynomial overflows intermediate or final when evaluating at a particular location
    template <std::ranges::range coeffs_t>
    constexpr auto overflows_at(coeffs_t const& coeffs, normalized_t t) const noexcept -> bool
        requires(is_fixed<std::ranges::range_value_t<coeffs_t>>)
    {
        using coeff_t = std::ranges::range_value_t<coeffs_t>;

        auto coeff = std::begin(coeffs);
        auto const coeffs_end = std::end(coeffs);

        auto accumulator = *coeff++;
        while (coeff != coeffs_end)
        {
            // get wide mac result
            auto const wide_value = mac(accumulator, t, *coeff++);

            // check for overflow
            if (overflows(wide_value)) return true;

            // accumulate
            accumulator = coeff_t::literal(int_cast<typename coeff_t::value_t>(wide_value));
        }

        return false;
    }

    // clamps real t to within [0, 1], convertss to fixed
    constexpr auto to_fixed(real_t t) const noexcept -> normalized_t
    {
        return crv::to_fixed<normalized_t>(std::min(static_cast<real_t>(1.0), t));
    }
};

} // namespace crv::spline::defect_detectors
